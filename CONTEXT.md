# Sleep Recovery Context

## Glossary

- **Sleep recovery assessment**: The MVP scenario for this project. It turns one or more nights of sleep-related data into an estimated recovery outcome for display on macOS.
- **Night session**: A single sleep session, usually one night, represented by a summary row and optional time-series rows.
- **Recovery label**: The primary MVP target. A 3-class label representing poor, average, or good recovery.
- **Weak label**: A label derived from rules instead of direct human annotation. In this project, weak labels are generated from wearable sleep CSV fields when a dataset does not include a native recovery target.
- **Feature package**: The precomputed model input bundle consumed by Swift. It contains normalized features for inference and standardized time series for charts.
- **Golden sample**: A fixed sample dataset, expected features, and expected prediction output used to verify consistency across C++, Python, Core ML, and Swift.
- **Model adapter**: The layer that normalizes feature order, label mapping, metadata, and export behavior across multiple candidate models.
- **Core ML export**: The step that converts the selected training artifact into a model that macOS can load locally.
- **Wearable sleep CSV**: A Kaggle-style dataset exported from a wearable or sleep tracking workflow, typically containing tabular sleep summary fields and sometimes heart rate or activity traces.

## Current Project Language

- Prefer **recovery** over **fatigue** when naming the primary output.
- Use **night session** for a single analyzed sleep event.
- Use **feature package** for the Swift-side inference input, not "raw pipeline input".
- Use **weak label** when the recovery target is rule-derived from Kaggle data.

## Current Architectural Shape

- `C++` owns data adaptation, normalization, synthetic fallback generation, and feature extraction.
- `Python / Colab` owns model training, model comparison, model adapter logic, and Core ML export validation.
- `SwiftUI` on macOS owns local inference consumption, chart rendering, and result presentation.
- `Core ML export` is treated as its own validation seam between training and app integration.
