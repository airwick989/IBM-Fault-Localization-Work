from pickle import TRUE
from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from flask_sqlalchemy import SQLAlchemy
import pandas as pd
import numpy as np
from sklearn.cluster import KMeans
from sklearn.cluster import DBSCAN
from sklearn.decomposition import PCA
from sklearn.preprocessing import StandardScaler
from sklearn.preprocessing import MinMaxScaler
from sklearn.metrics import silhouette_score
from sklearn.model_selection import train_test_split
import seaborn as sns
import matplotlib.pyplot as plt
from sklearn.preprocessing import LabelEncoder
from json import loads
from kafka import KafkaConsumer
import pickle

app = Flask(__name__)
CORS(app)
app.app_context().push()    #added to mitigate "working outside of application context" error

#initialise variables to be used as dataframes
jlm = None
perf = None
test = None

filesPath = './Files/Uploads/'
modelPath = './Files/Models/'


"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""

app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///../../../files.db'
fileDB = SQLAlchemy(app)

#Creates a database model, ie, a table. We will refer to this table as 'File'
class File(fileDB.Model):
    filename = fileDB.Column(fileDB.String(50) , primary_key = True)
    data = fileDB.Column(fileDB.LargeBinary)

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""



"""---- CLASSIFIER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""

def getFiles():
    global filesPath
    readfiles = fileDB.session.query(File)

    for file in readfiles:
        
        # Read binary data and convert it into a csv format
        data = file.data
        csv = str(data)[2:-1]
        csv = csv.replace("\\r\\n", "\n")   #'\r\n' if windows, just '\n' if linux?
        csv = csv.replace("\\xef\\xbb\\xbf", "")   #Clean some utf-8 escape characters
        
        print(csv, file=open(f'{filesPath}{file.filename}', 'w'))
        # if file.filename == "jlm.csv":
        #     data = file.data
        #     csv = str(data)[2:-1]
        #     print(csv[0:5000])

        #String to dataframe format
        # csvData = csv.split('\n')
        # for i in range(0,len(csvData)):
        #     csvData[i] = csvData[i].split(",")
        # headers = csvData[0]
        # csvData = csvData[1:-1]


def scale_standard(df):
    with open(f'{modelPath}scaler.pkl', 'rb') as f:
        scaler = pickle.load(f)
    df_scaled = scaler.transform(df)
    return df_scaled


def apply_pca(df):
    with open(f'{modelPath}pca.pkl', 'rb') as f:
        pca = pickle.load(f)
    features = pca.transform(df)
    return features


def processData():
    global filesPath
    DF = pd.concat([pd.read_csv(f, encoding='UTF-8') for f in [f'{filesPath}jlm.csv' ,f'{filesPath}test.csv', f'{filesPath}perf.csv']], axis=1)
    sum_columns = DF["TIER2"] + DF["TIER3"]
    DF["SPIN_COUNT"] = sum_columns
    DF["SLOW"].mean()/DF["GETS"].mean()
    DF["%UTIL"].mean()/100
    DF.drop(DF.columns[DF.columns.str.contains('Unnamed:',case = False)],axis = 1, inplace = True)
    DF1 = DF.copy()
    DF1 = DF1[['%MISS', 'GETS', 'SLOW', 'NONREC', 'REC', 'TIER2', 'TIER3', '%UTIL', 'AVER_HTM', 'PATTERN-NAME', 'PATTERN-NO', '_raw_spin_lock', 'ctx_sched_in', 'delay_mwaitx', 'THREADS', 'SLEEP', 'SPIN_COUNT']]
    DF1 = DF1.rename(columns = {'_raw_spin_lock' : 'RAW_SPIN_LOCK', 'ctx_sched_in' : 'CTX_SWITCH', 'delay_mwaitx' : 'DELAY_MWAITX'})
    DF_train = DF1[['GETS', 'SPIN_COUNT', 'NONREC', '%UTIL', 'AVER_HTM', 'PATTERN-NO', 'PATTERN-NAME', 'RAW_SPIN_LOCK', 'CTX_SWITCH', 'DELAY_MWAITX']]
    
    #THIS IS ADDED BY ME
    DF_trainBeforeDrop = DF_train
    DF_train = DF_train.drop(['PATTERN-NAME'], axis=1)
    
    cols = []
    for col in DF_train.columns:
            cols.append(col)
    DF_train[cols] = scale_standard(DF_train[cols])
    print(DF_train)
    pca_DF_train = pd.DataFrame(data = apply_pca(DF_train), columns = ['pc1', 'pc2'])

    classify(pca_DF_train)

def classify(pca_DF_train):
    with open(f'{modelPath}kmeans12.pkl', 'rb') as f:
        kmeans12 = pickle.load(f)

    predicted_clusters = []
    for index, row in pca_DF_train.iterrows():
        #print(kmeans12.predict([[row['pc1'], row['pc2']]])[0])
        predicted_clusters.append(kmeans12.predict([[row['pc1'], row['pc2']]])[0])
    pca_DF_train['Cluster'] = predicted_clusters
    print(pca_DF_train)

"""---- CLASSIFIER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""



"""---- KAFKA CONSUMER ------------------------------------------------------------------------------------------------------------------"""

def deserialize(message):
    try:
        return loads(message.decode('utf-8'))
    except Exception:
        return "Error: Message is not JSON Deserializable"

consumer = KafkaConsumer(
    'coordinatorToClassifier',
    bootstrap_servers = ['localhost:9092'],
    auto_offset_reset = 'latest',
    enable_auto_commit = True,
    group_id = None,
    value_deserializer = deserialize
)

"""---- KAFKA CONSUMER ------------------------------------------------------------------------------------------------------------------"""


for message in consumer:
    data = message.value
    if 'signal' in data:
        if data['signal'] == 'start':
            getFiles()
            processData()



# @app.route("/")
# def hello():

#     # for message in consumer:
#     #     data = message.value
#     #     print(data)

#     getFiles()
#     processData()
    
#     return "Hello World!"



# if __name__ == '__main__':
#     app.run(port=5001, debug=True)