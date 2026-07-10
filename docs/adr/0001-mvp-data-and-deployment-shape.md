# ADR-0001: MVP Data And Deployment Shape

## Status

Accepted

## Context

The project needs an MVP that demonstrates real engineering tradeoffs across data adaptation, model training, Core ML conversion, and macOS presentation. The project currently has no hardware collection pipeline, so the first production-like loop must rely on public data plus controlled fallback data generation.

The MVP must also demonstrate problem analysis, not just a happy-path demo. That means the architecture needs explicit room for:

- Kaggle data adaptation issues
- weak-label generation
- multi-model comparison
- Core ML deployment constraints
- a documented fallback path if the primary model family becomes too costly to ship

## Decision

The MVP adopts the following shape:

1. Use a Kaggle wearable sleep CSV dataset as the primary training data source.
2. Use rule-generated weak labels when the dataset does not provide a native recovery label.
3. Use precomputed feature packages as the Swift inference input instead of raw time-series feature extraction on device.
4. Use `XGBoost / LightGBM` as the primary deployment candidate.
5. Keep `Logistic Regression` and `Random Forest` as comparison models and as fallback references in case Core ML deployment cost becomes too high.
6. Treat `Core ML export and consistency validation` as a distinct seam between training and app integration.

## Consequences

### Positive

- The MVP can start before any real hardware integration exists.
- The project demonstrates practical data and deployment problem solving.
- Swift stays focused on inference and presentation, reducing duplicate feature logic.
- The fallback path is explicit instead of implicit.

### Negative

- Weak labels will not represent a medically validated recovery target.
- The data adaptation layer becomes more important and may expose dataset-specific compromises.
- `XGBoost / LightGBM` may require extra Core ML compatibility work compared with simpler baselines.

## Follow-up

- Select the concrete Kaggle dataset during implementation.
- Record model export tradeoffs once `XGBoost / LightGBM` conversion is tested.
- Revisit the issue tracker choice after the GitHub repository is created.
