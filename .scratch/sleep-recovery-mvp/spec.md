## Problem Statement

The user needs a production-shaped MVP for a sleep recovery assessment project that can run without any owned hardware devices. The MVP must show a realistic end-to-end engineering workflow: adapt public sleep data, clean and normalize it, derive a usable recovery target, train and compare models, convert a deployable model to Core ML, and present the result in a macOS app with charts.

The user does not want a toy demo that skips over real problems. The MVP needs to demonstrate practical problem solving around dataset mismatch, weak-label generation, model-family tradeoffs, Core ML deployment constraints, and cross-language integration boundaries between C++, Python, Core ML, and Swift.

## Solution

Build an MVP around a Kaggle wearable sleep CSV dataset as the primary training source. Use a C++ data adaptation and preprocessing pipeline to normalize the dataset into a stable project schema, and generate weak recovery labels when the dataset does not provide a native recovery target. Train multiple candidate classifiers in Python on Colab, compare them through a shared model adapter layer, and attempt Core ML deployment with XGBoost or LightGBM as the primary route.

On the product side, ship a macOS app that consumes a precomputed feature package plus standardized chart data. The app performs local Core ML inference and presents the result through sleep-recovery-oriented views: nightly trend charts, recovery classification, probability output, key contributing factors, and multi-night trend analysis.

If the primary XGBoost or LightGBM deployment route becomes too costly or brittle, the MVP keeps a documented fallback path using simpler comparison models, while preserving the engineering analysis of why the primary route was expensive.

## User Stories

1. As a project owner, I want to start the MVP without any wearable hardware, so that I can prove the pipeline using public datasets first.
2. As a project owner, I want to use a Kaggle wearable sleep CSV dataset, so that the MVP works with realistic exported data rather than purely synthetic samples.
3. As a data engineer, I want to map heterogeneous Kaggle fields into one internal schema, so that downstream training and deployment do not depend on dataset-specific column names.
4. As a data engineer, I want to normalize timestamps, units, and missing values, so that the resulting data is consistent enough for feature extraction and chart rendering.
5. As an ML engineer, I want to generate weak recovery labels when a dataset lacks a native target, so that the MVP can still train a recovery classifier.
6. As an ML engineer, I want to retain a synthetic fallback path, so that missing fields or sparse public data do not block the pipeline.
7. As an ML engineer, I want to extract a stable feature package from each night session, so that model training and Swift inference use the same contract.
8. As an ML engineer, I want to compare Logistic Regression, Random Forest, and XGBoost or LightGBM on the same features, so that model choice is explained rather than assumed.
9. As an ML engineer, I want one model adapter layer that standardizes feature order, label mapping, and metadata, so that different model families can be trained and exported with less duplication.
10. As an ML engineer, I want to validate whether the primary model family can be exported to Core ML, so that deployment risk is discovered before app integration.
11. As a macOS app user, I want the app to consume a precomputed feature package, so that inference is fast and does not require reimplementing feature extraction on device.
12. As a macOS app user, I want to see a nightly heart-rate trend chart, so that I can visually inspect how the night behaved.
13. As a macOS app user, I want to see a nightly activity trend chart, so that I can understand interruptions and movement across the night.
14. As a macOS app user, I want to see the predicted recovery label, so that I can quickly understand the model outcome.
15. As a macOS app user, I want to see classification probability or confidence information, so that the result feels more transparent than a single opaque label.
16. As a macOS app user, I want to see key contributing factors behind the prediction, so that the output feels actionable rather than purely statistical.
17. As a macOS app user, I want to see a multi-night recovery trend, so that the MVP reflects longitudinal use instead of a single isolated score.
18. As a project owner, I want a golden sample for cross-stack verification, so that C++, Python, Core ML, and Swift can be checked against the same expected result.
19. As a project owner, I want the MVP to document its tradeoffs and fallback paths, so that the project demonstrates engineering judgment, not only implementation speed.
20. As a future maintainer, I want the spec to separate preprocessing, training, Core ML validation, and app presentation into clear seams, so that the system can evolve without reworking every layer at once.

## Implementation Decisions

- The product scenario is sleep recovery assessment, not medical diagnosis and not full clinical sleep staging.
- The MVP uses a Kaggle wearable sleep CSV dataset as the primary source of training data.
- The system standardizes data into four conceptual layers: raw input, normalized time series, derived features, and labels.
- The primary supervised task for MVP is a three-class recovery classification target.
- If the dataset lacks a native recovery target, the system generates weak labels using project rules based on sleep duration, heart-rate behavior, activity interruptions, and similar available fields.
- Synthetic data remains in scope only as a fallback or augmentation source when a public dataset lacks a field needed for the pipeline contract.
- The implementation is split into four seams:
  - data adaptation and preprocessing in C++
  - training and model adaptation in Python / Colab
  - Core ML export and consistency validation
  - macOS presentation in SwiftUI
- Swift does not perform raw time-series feature extraction in MVP. It consumes a precomputed feature package and standardized chart data.
- The model adapter layer is responsible for normalizing feature order, label mapping, output metadata, and export-related configuration across different candidate models.
- Three candidate classifiers are compared in MVP: Logistic Regression, Random Forest, and XGBoost or LightGBM.
- XGBoost or LightGBM is the primary deployment route because it better demonstrates realistic model-deployment tradeoffs.
- Logistic Regression and Random Forest stay in scope as comparison models and documented fallback paths if Core ML conversion or runtime integration becomes too costly.
- Core ML export is treated as its own engineering seam instead of being hidden inside the training flow, because deployment risk is a first-class part of the project.
- The macOS MVP home view includes:
  - nightly heart-rate trend chart
  - nightly activity trend chart
  - predicted recovery label
  - probability or confidence display
  - key contributing factors
  - multi-night trend section
- Key contributing factors may be provided either by a lightweight contribution method supported by the chosen model family or by offline precomputed explanation fields if that yields a clearer MVP.
- The project maintains a golden sample package containing a canonical night session, expected features, and expected prediction output for cross-stack verification.
- Specs and tickets are published to the local markdown issue tracker during the current project phase.
- Project terminology is standardized around recovery, night session, feature package, weak label, model adapter, and golden sample.

## Testing Decisions

- Good tests validate externally visible behavior and contract stability, not implementation details or internal helper structure.
- The preprocessing seam should be tested through its observable outputs: normalized records, derived features, and schema consistency.
- The model-training seam should be tested through reproducible feature loading, stable label mapping, metric generation, and model metadata output.
- The Core ML seam should be tested through export success or failure characterization, output-shape validation, and numerical agreement against Python within an agreed tolerance.
- The macOS presentation seam should be tested through loading a feature package, producing a local inference result, and rendering the expected chart and result regions.
- The golden sample should be used as the common regression reference across C++, Python, Core ML, and Swift.
- Testing should prefer the highest seam possible:
  - verify preprocessing outputs rather than internal parsing helpers
  - verify model adapter behavior rather than per-model ad hoc glue
  - verify Python versus Core ML consistency rather than isolated conversion internals
  - verify Swift view-model and app behavior rather than presentation-only implementation details
- Prior art inside this repo does not exist yet, so the first tests also define the style baseline for later work.

## Out of Scope

- Medical diagnosis or clinically validated health recommendations
- Full sleep-stage modeling as an MVP requirement
- Real-time on-device feature extraction from raw wearable streams
- iOS deployment
- Hardware ingestion from Apple Watch or other physical devices
- Personalized baseline modeling across long user histories
- Automated synchronization to GitHub Issues during the current local-markdown phase

## Further Notes

- The spec intentionally keeps the MVP focused on one realistic public-data route plus a documented fallback path.
- The project should explicitly record which Kaggle dataset was chosen and why competing datasets were rejected.
- If the chosen dataset contains an existing sleep quality score, the implementation may compare score-mapping versus weak-label generation, but the MVP still needs one final authoritative labeling strategy.
- If XGBoost or LightGBM becomes too expensive to export or integrate, the fallback decision must be documented as an engineering tradeoff, not hidden as a silent simplification.
