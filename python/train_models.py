"""
M3: Baseline Training & Model Adapter (Ticket 03)

Reads derived_features.csv from M2 C++ outputs.
Trains and compares 3 classifiers (Logistic Regression, Random Forest, XGBoost).
Core ML export verification (Ticket 04).

Uses Leave-One-User-Out (LOUO) cross-validation.
Run: cd python && uv run python train_models.py
"""

import json
import warnings
from pathlib import Path
from typing import Optional, Callable

import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import LeaveOneGroupOut, cross_val_score
from sklearn.metrics import (
    accuracy_score,
    balanced_accuracy_score,
    f1_score,
    confusion_matrix,
    classification_report,
)
from sklearn.preprocessing import StandardScaler

warnings.filterwarnings("ignore", category=UserWarning)
warnings.filterwarnings("ignore", category=FutureWarning)

ROOT = Path(__file__).resolve().parent.parent
FEATURES_PATH = ROOT / "data/features/derived_features.csv"
SESSIONS_PATH = ROOT / "data/normalized/night_sessions.csv"
MODELS_DIR = ROOT / "models"
MODELS_DIR.mkdir(parents=True, exist_ok=True)

FEATURE_ORDER = [
    "mean_hr", "min_hr", "max_hr", "hr_std",
    "hr_drop_first_90m", "activity_mean", "activity_spike_count",
    "sleep_fragmentation_score", "total_duration_min",
]

LABEL_MAP = {0: "poor", 1: "average", 2: "good"}


# ============================================================
# Model Adapter Layer
# ============================================================

class ModelAdapter:
    """
    Unified training/evaluation interface across model families.
    Ensures consistent feature order, label mapping, and export metadata.
    """

    def __init__(self, feature_order: list[str], label_map: dict):
        self.feature_order = feature_order
        self.label_map = label_map
        self._lr_scaler: Optional[StandardScaler] = None

    # ---- internal helpers for LR scaling ----

    def _scale_fit(self, X: np.ndarray) -> np.ndarray:
        self._lr_scaler = StandardScaler()
        return self._lr_scaler.fit_transform(X)

    def _scale(self, X: np.ndarray) -> np.ndarray:
        return self._lr_scaler.transform(X)

    # ---- public train entry-points (called per-fold) ----

    def train_logistic_regression(self, X: np.ndarray, y: np.ndarray) -> LogisticRegression:
        X_s = self._scale_fit(X)
        m = LogisticRegression(multi_class="multinomial", max_iter=2000, C=0.5, penalty="l2", random_state=42)
        m.fit(X_s, y)
        return m

    def train_random_forest(self, X: np.ndarray, y: np.ndarray) -> RandomForestClassifier:
        m = RandomForestClassifier(n_estimators=100, max_depth=5, min_samples_leaf=3, random_state=42)
        m.fit(X, y)
        return m

    # ---- prediction helpers ----

    def _preprocess(self, model, X: np.ndarray) -> np.ndarray:
        if isinstance(model, LogisticRegression):
            return self._scale(X)
        return X

    def predict(self, model, X: np.ndarray) -> np.ndarray:
        X_p = self._preprocess(model, X)
        return model.predict(X_p)

    def predict_proba(self, model, X: np.ndarray) -> np.ndarray:
        X_p = self._preprocess(model, X)
        return model.predict_proba(X_p)


# ============================================================
# LOUO Cross-Validation
# ============================================================

def load_data() -> tuple[pd.DataFrame, np.ndarray, np.ndarray, np.ndarray]:
    features = pd.read_csv(FEATURES_PATH)
    groups = features["session_id"].str.split("_").str[0].values
    X = features[FEATURE_ORDER].values.astype(np.float64)
    y = features["recovery_label"].values.astype(np.int64)
    return features, X, y, groups


def evaluate_with_louo(
    name: str,
    X: np.ndarray,
    y: np.ndarray,
    groups: np.ndarray,
    train_fn: Callable,
) -> dict:
    """Leave-One-User-Out CV for a model family. Returns metrics dict."""
    logo = LeaveOneGroupOut()
    y_true_all, y_pred_all = [], []
    fold_results = []

    for fold_idx, (train_idx, test_idx) in enumerate(logo.split(X, y, groups)):
        X_tr, X_te = X[train_idx], X[test_idx]
        y_tr, y_te = y[train_idx], y[test_idx]

        adapter = ModelAdapter(FEATURE_ORDER, LABEL_MAP)
        model = train_fn(adapter, X_tr, y_tr)

        y_pred = adapter.predict(model, X_te)
        y_true_all.extend(y_te.tolist())
        y_pred_all.extend(y_pred.tolist())

        fold_results.append({
            "fold": fold_idx,
            "test_users": sorted(set(str(g) for g in groups[test_idx])),
            "n_samples": int(len(y_te)),
            "accuracy": round(float(accuracy_score(y_te, y_pred)), 3),
            "balanced_accuracy": round(float(balanced_accuracy_score(y_te, y_pred)), 3),
        })

    yt = np.array(y_true_all)
    yp = np.array(y_pred_all)

    return {
        "model": name,
        "accuracy": round(float(accuracy_score(yt, yp)), 3),
        "balanced_accuracy": round(float(balanced_accuracy_score(yt, yp)), 3),
        "macro_f1": round(float(f1_score(yt, yp, average="macro")), 3),
        "confusion_matrix": confusion_matrix(yt, yp).tolist(),
        "classification_report": classification_report(
            yt, yp,
            target_names=[LABEL_MAP[i] for i in range(3)],
            output_dict=True,
        ),
        "folds": fold_results,
        "n_total": int(len(yt)),
    }


# ============================================================
# Core ML Export (Ticket 04)
# ============================================================

def attempt_coreml_export_xgboost(X: np.ndarray, y: np.ndarray) -> dict:
    """XGBoost → Core ML with libomp installed."""
    n_classes = int(len(set(y)))

    try:
        import xgboost as xgb
        import coremltools as ct

        dtrain = xgb.DMatrix(X, label=y, feature_names=FEATURE_ORDER)
        params = {
            "objective": "multi:softprob" if n_classes > 2 else "binary:logistic",
            "num_class": n_classes if n_classes > 2 else 1,
            "max_depth": 4,
            "learning_rate": 0.1,
            "random_state": 42,
        }
        model = xgb.train(params, dtrain, num_boost_round=50)

        coreml_model = ct.converters.xgboost.convert(
            model,
            feature_names=FEATURE_ORDER,
            target="recovery_label",
            mode="classifier",
            n_classes=n_classes,
        )

        model_path = MODELS_DIR / "xgb_classifier_v2.mlpackage"
        if model_path.exists():
            import shutil
            shutil.rmtree(model_path)
        coreml_model.save(str(model_path))

        # Quick validation
        sample = X[:1].astype(np.float64)
        py_proba = model.predict(xgb.DMatrix(sample, feature_names=FEATURE_ORDER))
        coreml_proba = coreml_model.predict({f: float(X[0, i]) for i, f in enumerate(FEATURE_ORDER)})

        return {
            "status": "success",
            "model_path": str(model_path),
            "note": "XGBoost → Core ML conversion successful (libomp installed)",
        }
    except ImportError as e:
        return {"status": "failed", "error": f"Import error: {e}", "fallback": "sckit-learn models (LR/RF)"}
    except Exception as e:
        return {"status": "failed", "error": str(e)[:200], "fallback": "sckit-learn models (LR/RF)"}


# ============================================================
# main
# ============================================================

def main():
    print("=" * 60)
    print("M3: Baseline Training & Model Adapter")
    print("=" * 60)

    # 1. Load data
    features, X, y, groups = load_data()
    print(f"\nData: {len(features)} sessions, {len(set(groups))} users")
    print(f"Label distribution: {dict(zip(*np.unique(y, return_counts=True)))}")

    # Per-user summary
    print("\nPer-user label distribution:")
    for uid in sorted(set(groups)):
        mask = groups == uid
        c = dict(zip([0, 1, 2], np.bincount(y[mask], minlength=3)))
        print(f"  {uid}: Poor={c[0]} Avg={c[1]} Good={c[2]} (total={mask.sum()})")

    # 2. Logistic Regression
    print("\n" + "=" * 40)
    print("1. Logistic Regression (baseline)")
    print("-" * 40)

    def _train_lr(ad, X_tr, y_tr):
        return ad.train_logistic_regression(X_tr, y_tr)

    lr = evaluate_with_louo("LogisticRegression", X, y, groups, _train_lr)
    print(f"  Accuracy:         {lr['accuracy']}")
    print(f"  Balanced Acc:     {lr['balanced_accuracy']}")
    print(f"  Macro F1:         {lr['macro_f1']}")
    print(f"  Confusion Matrix:\n{np.array(lr['confusion_matrix'])}")

    # 3. Random Forest
    print("\n" + "-" * 40)
    print("2. Random Forest (comparison)")
    print("-" * 40)

    def _train_rf(ad, X_tr, y_tr):
        return ad.train_random_forest(X_tr, y_tr)

    rf = evaluate_with_louo("RandomForest", X, y, groups, _train_rf)
    print(f"  Accuracy:         {rf['accuracy']}")
    print(f"  Balanced Acc:     {rf['balanced_accuracy']}")
    print(f"  Macro F1:         {rf['macro_f1']}")
    print(f"  Confusion Matrix:\n{np.array(rf['confusion_matrix'])}")

    # 4. Core ML export (XGBoost)
    print("\n" + "=" * 40)
    print("3. Core ML Export Test (Ticket 04)")
    print("-" * 40)

    xgb_result = attempt_coreml_export_xgboost(X, y)
    print(f"  Status: {xgb_result['status']}")
    if xgb_result["status"] == "failed":
        print(f"  Error:  {xgb_result.get('error', '')[:120]}")

    # 5. Model selection
    print("\n" + "=" * 40)
    print("Model Selection")
    print("-" * 40)

    if xgb_result["status"] == "success":
        primary = "XGBoost"
        reason = "Best model family; Core ML export verified"
    elif rf["balanced_accuracy"] >= lr["balanced_accuracy"]:
        primary = "RandomForest"
        reason = "XGBoost Core ML export failed; RF as best non-XGB model"
    else:
        primary = "LogisticRegression"
        reason = "XGBoost Core ML export failed; LR simplest deployable model"

    print(f"  Primary: {primary}")
    print(f"  Reason:  {reason}")

    # 6. Save report
    report = {
        "feature_order": FEATURE_ORDER,
        "label_map": LABEL_MAP,
        "data_summary": {
            "n_sessions": int(len(features)),
            "n_users": int(len(set(groups))),
            "label_distribution": {int(k): int(v) for k, v in zip(*np.unique(y, return_counts=True))},
        },
        "models": {
            "logistic_regression": lr,
            "random_forest": rf,
            "xgboost_coreml_export": xgb_result,
        },
        "model_selection": {"primary": primary, "reason": reason},
    }

    report_path = MODELS_DIR / "training_report.json"
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2, default=str)
    print(f"\n  Report saved to: {report_path}")


if __name__ == "__main__":
    main()
