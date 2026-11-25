
import numpy as np
import tensorflow as tf
import matplotlib.pyplot as plt

from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import (
    Conv1D,
    MaxPooling1D,
    GlobalAveragePooling1D,
    Dense,
    Dropout
)
from tensorflow.keras.callbacks import EarlyStopping
from sklearn.metrics import classification_report, confusion_matrix

# =====================================
# 1. LOAD PREPROCESSED DATA
# =====================================

print("Loading V2 datasets...")

X_train = np.load("X_train_v2.npy")
y_train = np.load("y_train_v2.npy")

X_val = np.load("X_val_v2.npy")
y_val = np.load("y_val_v2.npy")

X_test = np.load("X_test_v2.npy")
y_test = np.load("y_test_v2.npy")

print("Training:", X_train.shape)
print("Validation:", X_val.shape)
print("Independent Testing:", X_test.shape)

# =====================================
# 2. BUILD CNN MODEL
# =====================================

print("\nBuilding CNN model...")

model = Sequential([
    Conv1D(32, kernel_size=3, activation="relu",
           input_shape=(80, 6)),

    MaxPooling1D(pool_size=2),

    Conv1D(64, kernel_size=3, activation="relu"),

    GlobalAveragePooling1D(),

    Dense(32, activation="relu"),

    Dropout(0.3),

    Dense(5, activation="softmax")
])

model.compile(
    optimizer="adam",
    loss="sparse_categorical_crossentropy",
    metrics=["accuracy"]
)

model.summary()

# =====================================
# 3. EARLY STOPPING
# =====================================

early_stop = EarlyStopping(
    monitor="val_loss",
    patience=8,
    restore_best_weights=True,
    verbose=1
)

# =====================================
# 4. TRAIN MODEL
# =====================================

print("\nStarting CNN training...")

history = model.fit(
    X_train,
    y_train,
    validation_data=(X_val, y_val),
    epochs=60,
    batch_size=8,
    callbacks=[early_stop],
    shuffle=True,
    verbose=1
)

# =====================================
# 5. SAVE MODEL
# =====================================

model.save("gesture_cnn_v2.keras")

print("\nModel saved as gesture_cnn_v2.keras")

# =====================================
# 6. INDEPENDENT TEST EVALUATION
# =====================================

print("\nEvaluating on independent dataset...")

test_loss, test_accuracy = model.evaluate(
    X_test,
    y_test,
    verbose=0
)

print("\n================================")
print("INDEPENDENT TEST RESULTS")
print("================================")

print(f"Test Accuracy: {test_accuracy * 100:.2f}%")
print(f"Test Loss: {test_loss:.4f}")

# =====================================
# 7. CLASSIFICATION REPORT
# =====================================

y_pred_prob = model.predict(X_test)
y_pred = np.argmax(y_pred_prob, axis=1)

class_names = ["DOWN", "IDLE", "LEFT", "RIGHT", "UP"]

print("\nClassification Report:\n")

print(classification_report(
    y_test,
    y_pred,
    labels=[0, 1, 2, 3, 4],
    target_names=class_names,
    zero_division=0
))

# =====================================
# 8. CONFUSION MATRIX
# =====================================

cm = confusion_matrix(
    y_test,
    y_pred,
    labels=[0, 1, 2, 3, 4]
)

print("Confusion Matrix:")
print(cm)

plt.figure(figsize=(7, 6))
plt.imshow(cm, cmap="Blues")
plt.title("Gesture CNN V2 - Confusion Matrix")
plt.xlabel("Predicted Gesture")
plt.ylabel("Actual Gesture")
plt.xticks(range(5), class_names, rotation=30)
plt.yticks(range(5), class_names)

for i in range(5):
    for j in range(5):
        plt.text(j, i, cm[i, j],
                 ha="center", va="center")

plt.colorbar()
plt.tight_layout()
plt.savefig("confusion_matrix_v2.png")
plt.show()

# =====================================
# 9. ACCURACY GRAPH
# =====================================

plt.figure(figsize=(8, 5))
plt.plot(history.history["accuracy"], label="Training Accuracy")
plt.plot(history.history["val_accuracy"], label="Validation Accuracy")
plt.title("CNN V2 - Accuracy")
plt.xlabel("Epoch")
plt.ylabel("Accuracy")
plt.legend()
plt.grid(True)
plt.savefig("accuracy_graph_v2.png")
plt.show()

# =====================================
# 10. LOSS GRAPH
# =====================================

plt.figure(figsize=(8, 5))
plt.plot(history.history["loss"], label="Training Loss")
plt.plot(history.history["val_loss"], label="Validation Loss")
plt.title("CNN V2 - Loss")
plt.xlabel("Epoch")
plt.ylabel("Loss")
plt.legend()
plt.grid(True)
plt.savefig("loss_graph_v2.png")
plt.show()

print("\n================================")
print("CNN V2 TRAINING COMPLETED")
print("================================")
