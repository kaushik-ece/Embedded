"""Verify the distributed gesture recordings and summarize the archived test matrix."""

import csv
import math
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLASSES = ("DOWN", "IDLE", "LEFT", "RIGHT", "UP")
AXES = ("ax", "ay", "az", "gx", "gy", "gz")


def read_recordings(name, expected_total, per_class):
    path = ROOT / "data" / name
    labels = {}
    lengths = Counter()
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ["sample_id", "timestamp", *AXES, "label"]:
            raise ValueError(f"{name}: unexpected columns")
        for line, row in enumerate(reader, start=2):
            key = int(row["sample_id"])
            label = row["label"]
            if label not in CLASSES or labels.setdefault(key, label) != label:
                raise ValueError(f"{name}:{line}: inconsistent or unknown label")
            if None in row or not all(math.isfinite(float(row[k])) for k in ("timestamp", *AXES)):
                raise ValueError(f"{name}:{line}: invalid sensor record")
            lengths[key] += 1
    if sum(lengths.values()) != expected_total or Counter(labels.values()) != dict.fromkeys(CLASSES, per_class):
        raise ValueError(f"{name}: recording or row counts differ from archive")
    print(f"{name}: {len(labels)} recordings, {expected_total:,} readings, "
          f"{per_class}/class, {min(lengths.values())}-{max(lengths.values())} readings/recording")


def read_test_matrix():
    path = ROOT / "results" / "confusion_matrix.csv"
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ["actual", *CLASSES]:
            raise ValueError("Unexpected confusion matrix header")
        rows = list(reader)
    if [row["actual"] for row in rows] != list(CLASSES):
        raise ValueError("Unexpected true classes")
    correct = 0
    total = 0
    for row in rows:
        counts = {label: int(row[label]) for label in CLASSES}
        if min(counts.values()) < 0 or sum(counts.values()) != 30:
            raise ValueError("Expected 30 test recordings for each class")
        correct += counts[row["actual"]]
        total += sum(counts.values())
    print(f"Archived test matrix: {correct}/{total} correct ({100 * correct / total:.2f}%)")


if __name__ == "__main__":
    read_recordings("gesture_dataset.csv", expected_total=8723, per_class=20)
    read_recordings("gesture_dataset_new.csv", expected_total=32840, per_class=30)
    read_test_matrix()
