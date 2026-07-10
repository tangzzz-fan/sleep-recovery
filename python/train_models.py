"""
M3: Baseline Training & Model Adapter (Ticket 03)

Reads derived_features.csv from M2 C++ outputs.
Trains and compares 3 classifiers:
  1. Logistic Regression (baseline, simplest Core ML export)
  2. Random Forest (comparison model)
  3. XGBoost (primary deployment candidate)

Uses Leave-One-User-Out (LOUO) cross-validation to handle
the small sample size (12 users, ~240 sessions).

Run:
    cd python && uv run python train_models.py
"""

import json
import warnings
from pathlib import Path
from typing import Optional

import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import LeaveOneGroupOut
from sklearn.metrics import (
    accuracy_score,
    balanced_accuracy_score,
    f1_score,
    confusion_matrix,
    classification_report,
)
from sklearn.preprocessing import StandardScaler

warnings.filterwarnings("ignore", category=UserWarning)

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
    Unified interface for training and evaluating different model families.
    Ensures consistent feature order, label mapping, and metadata.
    """

    def __init__(self, feature_order: list[str], label_map: dict):
        self.feature_order = feature_order
        self.label_map = label_map
        self.scaler: Optional[StandardScaler] = None

    def prepare_features(self, df: pd.DataFrame) -> np.ndarray:
        """Ensure features are in the locked order from data-contract.md."""
        return df[self.feature_order].values.astype(np.float64)

    def scale(self, X: np.ndarray, fit: bool = False) -> np.ndarray:
        """Standardize features (only for Logistic Regression)."""
        if fit:
            self.scaler = StandardScaler()
            return self.scaler.fit_transform(X)
        elif self.scaler is not None:
            return self.scaler.transform(X)
        return X

    def train_lr(self, X: np.ndarray, y: np.ndarray) -> LogisticRegression:
        X_scaled = self.scale(X, fit=True)
        model = LogisticRegression(
            multi_class="multinomial",
            max_iter=2000,
            C=0.5,
            penalty="l2",
            random_state=42,
        )
        model.fit(X_scaled, y)
        return model

    def train_rf(self, X: np.ndarray, y: np.ndarray) -> RandomForestClassifier:
        model = RandomForestClassifier(
            n_estimators=100,
            max_depth=5,
            min_samples_leaf=3,
            random_state=42,
        )
        model.fit(X, y)
        return model

    def predict(self, model, X: np.ndarray) -> np.ndarray:
        """Apply normalization during prediction — same as training."""
        if isinstance(model, LogisticRegression):
            X = self.scale(X, fit=False)
        return model.predict(X)

    def predict_proba(self, model, X: np.ndarray) -> np.ndarray:
        if isinstance(model, LogisticRegression):
            X = self.scale(X, fit=False)
        return model.predict_proba(X)


# ============================================================
# LOUO Cross-Validation
# ============================================================

def load_data() -> tuple[pd.DataFrame, np.ndarray, np.ndarray]:
    """Load features and extract user IDs from session_id for LOUO splits."""
    features = pd.read_csv(FEATURES_PATH)
    # Extract user_id from session_id (format: userid_date)
    groups = features["session_id"].str.split("_").str[0].values
    X = features[FEATURE_ORDER].values.astype(np.float64)
    y = features["recovery_label"].values.astype(np.int64)
    return features, X, y, groups


def evaluate_model(name: str, adapter: ModelAdapter, X: np.ndarray, y: np.ndarray,
                   groups: np.ndarray, train_func) -> dict:
    """
    Evaluate a model with Leave-One-User-Out CV.
    Returns dict with metrics, feature importance, and per-fold results.
    """
    logo = LeaveOneGroupOut()
    y_true_all = []
    y_pred_all = []
    y_prob_all = []
    fold_results = []

    for fold_idx, (train_idx, test_idx) in enumerate(logo.split(X, y, groups)):
        X_train, X_test = X[train_idx], X[test_idx]
        y_train, y_test = y[train_idx], y[test_idx]

        # Re-create adapter for each fold (fresh scaler)
        fold_adapter = ModelAdapter(FEATURE_ORDER, LABEL_MAP)
        model = train_func(fold_adapter, X_train, y_train)

        y_pred = fold_adapter.predict(model, X_test)
        y_prob = fold_adapter.predict_proba(model, X_test)

        y_true_all.extend(y_test.tolist())
        y_pred_all.extend(y_pred.tolist())
        y_prob_all.extend(y_prob.tolist())

        fold_acc = accuracy_score(y_test, y_pred)
        fold_bal = balanced_accuracy_score(y_test, y_pred)
        fold_results.append({
            "fold": fold_idx,
            "test_users": list(set(groups[test_idx])),
            "n_samples": len(y_test),
            "accuracy": round(fold_acc, 3),
            "balanced_accuracy": round(fold_bal, 3),
        })

    y_true_all = np.array(y_true_all)
    y_pred_all = np.array(y_pred_all)

    cm = confusion_matrix(y_true_all, y_pred_all)

    return {
        "model": name,
        "accuracy": round(accuracy_score(y_true_all, y_pred_all), 3),
        "balanced_accuracy": round(balanced_accuracy_score(y_true_all, y_pred_all), 3),
        "macro_f1": round(f1_score(y_true_all, y_pred_all, average="macro"), 3),
        "confusion_matrix": cm.tolist(),
        "classification_report": classification_report(
            y_true_all, y_pred_all,
            target_names=[LABEL_MAP[i] for i in range(3)],
            output_dict=True,
        ),
        "folds": fold_results,
        "n_total": len(y_true_all),
    }


# ============================================================
# Core ML Export Verification (Ticket 04)
# ============================================================

def attempt_coreml_export_xgboost(adapter: ModelAdapter, X: np.ndarray, y: np.ndarray):
    """Attempt XGBoost → Core ML conversion. Returns dict with result."""

    # Dynamically determine n_classes
    n_classes = len(set(y))

    try:
        import xgboost as xgb

        X_scaled = adapter.scale(X, fit=True)
        X_scaled = adapter.scale(X, fit=False)  # re-fit for export
        # Actually, use raw X for tree-based models
        dtrain = xgb.DMatrix(X, label=y)

        params = {
            "objective": "multi:softprob" if n_classes > 2 else "binary:logistic",
            "num_class": n_classes if n_classes > 2 else 1,
            "max_depth": 4,
            "learning_rate": 0.1,
            "n_estimators": 50,
            "random_state": 42,
        }
        model = xgb.train(params, dtrain, num_boost_round=50)

        # Attempt Core ML conversion
        import coremltools as ct

        coreml_model = ct.converters.xgboost.convert(
            model,
            feature_names=FEATURE_ORDER,
            target="recovery_label",
            mode="classifier",
            n_classes=n_classes,
        )

        # Test prediction on a sample
        sample = X[:1].astype(np.float32)
        coreml_pred = coreml_model.predict({"input": sample})

        # Save the model
        model_path = MODELS_DIR / "xgboost_coreml_test.mlpackage"
        coreml_model.save(str(model_path))

        return {
            "status": "success",
            "model_path": str(model_path),
            "test_prediction_shape": str(coreml_pred.shape) if hasattr(coreml_pred, "shape") else "ok",
            "note": "XGBoost → Core ML conversion successful",
        }
    except ImportError as e:
        return {"status": "failed", "error": f"import error: {e}", "fallback": "LogisticRegression"}
    except Exception as e:
        return {"status": "failed", "error": str(e), "fallback": "RandomForest via scikit-learn"}


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
    print(f"Features: {FEATURE_ORDER}")
    print(f"Label distribution: {dict(zip(*np.unique(y, return_counts=True)))}")

    # Summary stats per user
    user_labels = []
    for uid in sorted(set(groups)):
        mask = groups == uid
        counts = dict(zip([0, 1, 2], np.bincount(y[mask], minlength=3)))
        user_labels.append({"user": uid, **counts, "total": mask.sum()})

    print("\nPer-user label distribution:")
    for ul in user_labels:
        print(f"  {ul['user']}: Poor={ul[0]} Avg={ul[1]} Good={ul[2]} (total={ul['total']})")

    # 2. Model adapter
    adapter = ModelAdapter(FEATURE_ORDER, LABEL_MAP)

    # 3. Evaluate Logistic Regression
    print("\n" + "=" * 40)
    print("1. Logistic Regression (baseline)")
    print("-" * 40)

    def train_lr_fn(ad, X_tr, y_tr):
        return ad.train_lr(X_tr, y_tr)

    lr_results = evaluate_model("LogisticRegression", adapter, X, y, groups, train_lr_fn)
    print(f"  Accuracy:         {lr_results['accuracy']}")
    print(f"  Balanced Acc:     {lr_results['balanced_accuracy']}")
    print(f"  Macro F1:         {lr_results['macro_f1']}")
    print(f"  Confusion Matrix:\n{np.array(lr_results['confusion_matrix'])}")

    # 4. Evaluate Random Forest
    print("\n" + "-" * 40)
    print("2. Random Forest (comparison)")
    print("-" * 40)

    def train_rf_fn(ad, X_tr, y_tr):
        return ad.train_rf(X_tr, y_tr)

    rf_results = evaluate_model("RandomForest", adapter, X, y, groups, train_rf_fn)
    print(f"  Accuracy:         {rf_results['accuracy']}")
    print(f"  Balanced Acc:     {rf_results['balanced_accuracy']}")
    print(f"  Macro F1:         {rf_results['macro_f1']}")
    print(f"  Confusion Matrix:\n{np.array(rf_results['confusion_matrix'])}")

    # 5. Core ML export test
    print("\n" + "=" * 40)
    print("3. Core ML Export Test (Ticket 04)")
    print("-" * 40)

    xgb_result = attempt_coreml_export_xgboost(adapter, X, y)
    print(f"  Status: {xgb_result['status']}")
    if xgb_result['status'] == 'failed':
        print(f"  Error:  {xgb_result['error'][:120]}...")
        print(f"  Fallback: {xgb_result['fallback']}")

    # 6. Determine primary model
    print("\n" + "=" * 40)
    print("Model Selection")
    print("-" * 40)

    if xgb_result["status"] == "success":
        primary = "XGBoost"
        reason = "Best model family; Core ML export verified"
    elif rf_results["balanced_accuracy"] >= lr_results["balanced_accuracy"]:
        primary = "RandomForest"
        reason = "XGBoost Core ML export failed; RF as best non-XGB model"
    else:
        primary = "LogisticRegression"
        reason = "XGBoost Core ML export failed; LR simplest deployable model"

    print(f"  Primary: {primary}")
    print(f"  Reason:  {reason}")

    # 7. Save results
    report = {
        "feature_order": FEATURE_ORDER,
        "label_map": LABEL_MAP,
        "data_summary": {
            "n_sessions": len(features),
            "n_users": int(len(set(groups))),
            "label_distribution": {int(k): int(v) for k, v in zip(*np.unique(y, return_counts=True))},
        },
        "models": {
            "logistic_regression": lr_results,
            "random_forest": rf_results,
            "xgboost_coreml_export": xgb_result,
        },
        "model_selection": {
            "primary": primary,
            "reason": reason,
        },
    }

    report_path = MODELS_DIR / "training_report.json"
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2, default=str)
    print(f"\n  Report saved to: {report_path}")


if __name__ == "__main__":
    main()
