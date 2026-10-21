import matplotlib.pyplot as plt
import pandas as pd
from sklearn.tree import DecisionTreeClassifier, plot_tree, export_text
import joblib

#converting csv to dataframe
df = pd.read_csv("tomato.csv")

#marking inputfeatures
features = [
    "initial_surface",  #intial value of surface soil sensor
    "final_surface",    #final value of surface soil sensor
    "initial_root",     #intial value of root soil sensor
    "final_root",       #final value of root soil sensor
    "surface_change",   #change in initial and final value of the surface sensor in the given duration
    "root_change",      #change in initial and final value of the root sensor in the given duration
    "pressure",         #reading of the pressure sensor
    "avg_flow",         #reading of the flow sensor
]

target = "condition"  #outputrequired

#spliting the features and target from the train and test data

# train_data = df[df["split"].str.lower() == "train"]
# test_data  = df[df["split"].str.lower() == "test"]

#spliting the features and target from the train and test data

X_train = df[features]
y_train = df[target]

# X_test = test_data[features]
# y_test = test_data[target]

#training the model
model = DecisionTreeClassifier(max_depth=7,random_state=42)

model.fit(X_train, y_train)

# 5. Evaluate the trained model on the existing test data
# y_pred = model.predict(X_test)

# #calculatig accuracy o
# n the test data
# accu=accuracy_score(y_pred, y_test)

#dumping the model for future use in different file
import joblib

joblib.dump(model, 'model.pkl')
#\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
plt.figure(figsize=(26, 14))

plot_tree(
    model,
    feature_names=features,
    class_names=model.classes_,
    filled=True,
    rounded=True,
    fontsize=8
)

plt.title("RootSense Irrigation Diagnosis Decision Tree")
plt.tight_layout()
plt.show()
