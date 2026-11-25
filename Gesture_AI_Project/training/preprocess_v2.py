
import pandas as pd
import numpy as np

from sklearn.model_selection import train_test_split

# ==========================================
# 1. CONFIGURATION
# ==========================================

OLD_FILE = "gesture_dataset.csv"
NEW_FILE = "gesture_dataset_new.csv"

WINDOW_SIZE = 80

FEATURES = ["ax", "ay", "az", "gx", "gy", "gz"]

LABEL_MAP = {
    "DOWN": 0,
    "IDLE": 1,
    "LEFT": 2,
    "RIGHT": 3,
    "UP": 4
}

# ==========================================
# 2. LOAD DATASETS
# ==========================================

print("Loading original dataset...")

old_df = pd.read_csv(OLD_FILE)

print("Loading new dataset...")

new_df = pd.read_csv(NEW_FILE)

# ==========================================
# 3. VALIDATE DATA
# ==========================================

required_columns = [
    "sample_id", "timestamp", "label"
] + FEATURES

for name, df in [
    ("Original", old_df),
    ("New", new_df)
]:

    if not all(col in df.columns for col in required_columns):
        raise ValueError(
            f"{name} dataset has missing columns!"
        )

    if df[required_columns].isnull().any().any():
        raise ValueError(
            f"{name} dataset contains missing values!"
        )

    if not set(df["label"]).issubset(LABEL_MAP):
        raise ValueError(
            f"{name} dataset contains unknown labels!"
        )

    if df.groupby("sample_id")["label"].nunique().max() != 1:
        raise ValueError(
            f"{name} dataset has inconsistent recording labels!"
        )

print("Dataset validation successful!")

# ==========================================
# 4. CONVERT RECORDINGS TO FIXED WINDOWS
# ==========================================

def prepare_sequences(df):

    X = []
    y = []

    for sample_id, group in df.groupby(
        "sample_id",
        sort=True
    ):

        group = group.sort_values("timestamp")

        label = group["label"].iloc[0]

        values = group[FEATURES].to_numpy(
            dtype=np.float32
        )

        if len(values) < 2:
            raise ValueError(
                f"Recording {sample_id} is too short!"
            )

        old_positions = np.linspace(
            0, 1, len(values)
        )

        new_positions = np.linspace(
            0, 1, WINDOW_SIZE
        )

        sequence = np.zeros(
            (WINDOW_SIZE, len(FEATURES)),
            dtype=np.float32
        )

        for i in range(len(FEATURES)):

            sequence[:, i] = np.interp(
                new_positions,
                old_positions,
                values[:, i]
            )

        X.append(sequence)
        y.append(LABEL_MAP[label])

    return (
        np.array(X, dtype=np.float32),
        np.array(y, dtype=np.int64)
    )


# ==========================================
# 5. PROCESS BOTH DATASETS
# ==========================================

print("\nProcessing original dataset...")

X_original, y_original = prepare_sequences(old_df)

print("Processing new dataset...")

X_new, y_new = prepare_sequences(new_df)

print("\nOriginal dataset:", X_original.shape)
print("New dataset:", X_new.shape)

# ==========================================
# 6. SPLIT ORIGINAL DATASET
# ==========================================

indices = np.arange(len(X_original))

train_idx, val_idx = train_test_split(
    indices,
    test_size=0.20,
    random_state=42,
    stratify=y_original
)

X_train = X_original[train_idx]
y_train = y_original[train_idx]

X_val = X_original[val_idx]
y_val = y_original[val_idx]

# ==========================================
# 7. INDEPENDENT TEST DATA
# ==========================================

X_test = X_new
y_test = y_new

# ==========================================
# 8. NORMALIZATION
# ==========================================

mean = X_train.mean(axis=(0, 1))

std = X_train.std(axis=(0, 1))

std[std == 0] = 1.0

X_train = (X_train - mean) / std

X_val = (X_val - mean) / std

X_test = (X_test - mean) / std

# ==========================================
# 9. SAVE PROCESSED DATA
# ==========================================

np.save("X_train_v2.npy", X_train)
np.save("y_train_v2.npy", y_train)

np.save("X_val_v2.npy", X_val)
np.save("y_val_v2.npy", y_val)

np.save("X_test_v2.npy", X_test)
np.save("y_test_v2.npy", y_test)

np.savez(
    "scaler_params_v2.npz",
    mean=mean,
    std=std
)

# ==========================================
# 10. FINAL SUMMARY
# ==========================================

print("\n================================")
print("PREPROCESSING V2 COMPLETED")
print("================================")

print("Training:", X_train.shape)
print("Validation:", X_val.shape)
print("Independent Testing:", X_test.shape)

print("\nGesture mapping:")

for name, number in LABEL_MAP.items():
    print(name, "=", number)

print("\nAll processed files saved successfully!")
