# LTR Qt Capture Tool

Qt5 application for working with LTR114/LTR212 modules.

## External dependencies

The `LTR/` directory is intentionally not tracked. It contains LTRAPI headers,
DLLs and import libraries from the module vendor.

## Project layout

- `src/app/` contains the application entry point.
- `src/ui/` contains the main window implementation and Qt Designer form.
- `src/ltr/` contains LTR crate/module wrappers and LTR result helpers.
- `src/acquisition/` contains worker and synchronization timeline logic.
- `src/io/` contains measurement output code.
- `tests/` contains Qt test projects.

