<div align="right">

[中文](README.md) | [English](README_EN.md)

</div>

# RK1808 Face Attendance System

This repository is a face attendance demo for the Rockchip RK1808 Linux platform. It captures live camera frames, uses RockX/RKNN for face detection, landmark alignment, and face feature matching, and provides an on-device DRM display interface for face enrollment, attendance check-in, administrator record browsing, deletion, and manual record correction.

## Overview

- Target platform: RK1808 / RK1808 Linux device
- Language: C
- Core features: camera capture, DRM display, touch interaction, RockX face recognition, BMP-based record storage
- Runtime entry: enter `app-runtime` on the device and run `f1`
- Model dependency: `rockx-sdk-rk1808-linux` is included with headers, shared libraries, and face model data

## Repository Structure

```text
.
├── app-runtime/                 # Device runtime directory
├── source/                      # Source and development utilities
└── rockx-sdk-rk1808-linux/       # RockX/RKNN SDK and model files
```

## Main Functions

1. Camera capture and display on a DRM screen.
2. Face detection, alignment, and feature extraction through RockX.
3. Face enrollment and local face database management.
4. Attendance matching and BMP record saving.
5. Administrator operations for viewing, deleting, and correcting records.
