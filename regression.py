import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns

from sklearn.model_selection import train_test_split, cross_val_score, GridSearchCV
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.linear_model import LogisticRegression
from sklearn.tree import DecisionTreeClassifier, plot_tree
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score
from imblearn.over_sampling import SMOTE
from sklearn.feature_selection import RFE

# Suppress warnings for cleaner output
import warnings
warnings.filterwarnings('ignore')

# 1. Load the Dataset
df = pd.read_csv('perf_data.csv')

# 2. Data Aggregation: Combine multiple runs per configuration
# Assuming 3 runs per configuration
aggregated_df = df.groupby(['Program', 'Mode', 'Threads', 'Data_Size']).agg({
    'cache_references': ['mean', 'std'],
    'cache_misses': ['mean', 'std'],
    'L1_dcache_loads': ['mean', 'std'],
    'L1_dcache_load_misses': ['mean', 'std'],
    'L1_dcache_prefetches': ['mean', 'std'],
    'dTLB_loads': ['mean', 'std'],
    'dTLB_load_misses': ['mean', 'std'],
    'branch_instructions': ['mean', 'std'],
    'branch_misses': ['mean', 'std'],
    'context_switches': ['mean', 'std'],
    'cpu_migrations': ['mean', 'std'],
    'stalled_cycles_backend': ['mean', 'std'],
    'stalled_cycles_frontend': ['mean', 'std'],
    'cpu_cycles': ['mean', 'std'],
    'instructions': ['mean', 'std'],
    'elapsed_time': ['mean', 'std'],
    'user_time': ['mean', 'std'],
    'sys_time': ['mean', 'std']
}).reset_index()

# Flatten MultiIndex columns
aggregated_df.columns = ['_'.join(col).strip('_') for col in aggregated_df.columns.values]

# 3. Handle Missing Values (if any)
aggregated_df.dropna(inplace=True)

# 4. Encode the Target Variable
label_encoder = LabelEncoder()
aggregated_df['Mode_encoded'] = label_encoder.fit_transform(aggregated_df['Mode_mean'])  # Assuming 'Mode' is the first column
# Note: Adjust 'Mode_mean' based on your actual column naming after aggregation

# 5. Define Features and Target
# Exclude 'Program', 'Mode_mean', and 'Mode_std' if present
feature_columns = [col for col in aggregated_df.columns if col not in ['Program_mean', 'Program_std', 'Mode_mean', 'Mode_std', 'Mode_encoded']]

X = aggregated_df[feature_columns]
y = aggregated_df['Mode_encoded']

# 6. Split the Data
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, stratify=y, random_state=42
)

# 7. Normalize the Features
scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train)
X_test_scaled = scaler.transform(X_test)

# 8. Handle Class Imbalance using SMOTE
smote = SMOTE(random_state=42)
X_train_res, y_train_res = smote.fit_resample(X_train_scaled, y_train)

# 9. Feature Selection using Lasso Logistic Regression
logreg = LogisticRegression(penalty='l1', solver='saga', multi_class='multinomial', max_iter=10000, random_state=42)
logreg.fit(X_train_res, y_train_res)

# Get absolute coefficients
coef = np.abs(logreg.coef_)

# Average the coefficients across classes
avg_coef = np.mean(coef, axis=0)

# Create a DataFrame for feature importances
feature_importance = pd.DataFrame({
    'Feature': feature_columns,
    'Importance': avg_coef
}).sort_values(by='Importance', ascending=False)

# Plot Feature Importances
plt.figure(figsize=(12, 8))
sns.barplot(x='Importance', y='Feature', data=feature_importance)
plt.title('Feature Importance from Lasso Logistic Regression')
plt.xlabel('Average Absolute Coefficient')
plt.ylabel('Feature')
plt.tight_layout()
plt.show()

# Select features with importance above a threshold (e.g., 0.01)
threshold = 0.01
selected_features = feature_importance[feature_importance['Importance'] > threshold]['Feature'].tolist()
print(f"Selected Features (Importance > {threshold}):")
print(selected_features)

# 10. Optional: Recursive Feature Elimination (RFE) with Decision Tree
dt_clf_rfe = DecisionTreeClassifier(criterion='entropy', random_state=42)
rfe = RFE(estimator=dt_clf_rfe, n_features_to_select=10)
rfe.fit(X_train_res, y_train_res)

rfe_selected_features = [feature for feature, support in zip(feature_columns, rfe.support_) if support]
print("Selected Features via RFE:")
print(rfe_selected_features)

# Combine both feature selection methods
final_selected_features = list(set(selected_features + rfe_selected_features))
print("Final Selected Features:")
print(final_selected_features)

# 11. Retrain the Decision Tree Classifier with Selected Features
# Extract selected features from the scaled training and testing data
X_train_final = X_train[final_selected_features]
X_test_final = X_test[final_selected_features]

# Scale the selected features
X_train_final_scaled = scaler.fit_transform(X_train_final)
X_test_final_scaled = scaler.transform(X_test_final)

# Handle class imbalance again on the new selected features
X_train_final_res, y_train_final_res = smote.fit_resample(X_train_final_scaled, y_train)

# Initialize and Train the Decision Tree Classifier
dt_clf = DecisionTreeClassifier(criterion='entropy', random_state=42)
dt_clf.fit(X_train_final_res, y_train_final_res)

# 12. Model Evaluation
y_pred = dt_clf.predict(X_test_final_scaled)

# Classification Report
print("Classification Report:")
print(classification_report(y_test, y_pred, target_names=label_encoder.classes_))

# Confusion Matrix
conf_matrix = confusion_matrix(y_test, y_pred)
print("Confusion Matrix:")
print(conf_matrix)

# Plot Confusion Matrix
plt.figure(figsize=(8,6))
sns.heatmap(conf_matrix, annot=True, fmt='d', cmap='Blues',
            xticklabels=label_encoder.classes_,
            yticklabels=label_encoder.classes_)
plt.ylabel('Actual')
plt.xlabel('Predicted')
plt.title('Confusion Matrix - Decision Tree Classifier')
plt.show()

# 13. Cross-Validation
cv_scores = cross_val_score(dt_clf, X_train_final_res, y_train_final_res, cv=5, scoring='accuracy')
print(f"Cross-Validation Accuracy Scores: {cv_scores}")
print(f"Mean Accuracy: {cv_scores.mean()*100:.2f}% ± {cv_scores.std()*100:.2f}%")

# 14. Visualize the Decision Tree
plt.figure(figsize=(20,10))
plot_tree(dt_clf, feature_names=final_selected_features, class_names=label_encoder.classes_,
          filled=True, rounded=True, fontsize=12)
plt.title('Decision Tree Classifier')
plt.show()

# 15. Save the Model and Scaler for Future Use
import joblib

joblib.dump(dt_clf, 'decision_tree_classifier.pkl')
joblib.dump(scaler, 'scaler.pkl')
joblib.dump(label_encoder, 'label_encoder.pkl')
joblib.dump(feature_importance, 'feature_importance.pkl')

print("Model, scaler, label encoder, and feature importance have been saved.")
