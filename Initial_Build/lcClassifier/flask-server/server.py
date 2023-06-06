from pickle import TRUE
from flask import Flask, request
from flask_cors import CORS;
import pandas as pd
from json import loads, dumps
import pickle
from confluent_kafka import Consumer, Producer
import requests
import os
import zipfile
import shutil

app = Flask(__name__)
CORS(app)
app.app_context().push()    #added to mitigate "working outside of application context" error

#initialise variables to be used as dataframes
jlm = None
perf = None
test = None

filesPath = './Files/Uploads/'
modelPath = './Files/Models/'
zipPath = f"{filesPath}files.zip"
embeddedUploadsPath = f"{filesPath}Uploads/"


"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""

# app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
# app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///../../../files.db'
# fileDB = SQLAlchemy(app)

# #Creates a database model, ie, a table. We will refer to this table as 'File'
# class File(fileDB.Model):
#     filename = fileDB.Column(fileDB.String(50) , primary_key = True)
#     data = fileDB.Column(fileDB.LargeBinary)

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""



"""---- CLASSIFIER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""

def getFiles():
    global filesPath
    #readfiles = fileDB.session.query(File)
    params = {
        'targetExtensions': '.csv'
    }
    response = requests.get('http://localhost:5001/cds/getFiles', params=params)

    #Clear Uploads directory
    if len(os.listdir(filesPath)) > 0:
        for file in os.listdir(filesPath):
            #Checks for uploads directory in the directory and clears it
            if os.path.isdir(f"{filesPath}{file}"):
                shutil.rmtree(embeddedUploadsPath)
            else:
                os.remove(f'{filesPath}{file}')
    #Write zip file
    open(zipPath, "wb").write(response.content)

    #Extract all files from zip
    with zipfile.ZipFile(zipPath, 'r') as zip:
        zip.extractall(filesPath)
    #Move files to correct directory and remove unecessary folder
    if os.path.isdir(embeddedUploadsPath):
        for file in os.listdir(embeddedUploadsPath):
            os.rename(f"{embeddedUploadsPath}{file}", f"{filesPath}{file}")
        shutil.rmtree(embeddedUploadsPath)



    # for file in readfiles:
        
    #     if file.filename.endswith(".csv"):
    #         # Read binary data and convert it into a csv format
    #         data = file.data
    #         csv = str(data)[2:-1]

    #         #Windows
    #         #csv = csv.replace("\\r\\n", "\n")   #'\r\n' if windows, just '\n' if linux?

    #         #Linux
    #         csv = csv.replace("\\n", "\n")   #'\r\n' if windows, just '\n' if linux?

            
    #         csv = csv.replace("\\xef\\xbb\\xbf", "")   #Clean some utf-8 escape characters
            
    #         print(csv, file=open(f'{filesPath}{file.filename}', 'w'))
    #         # if file.filename == "jlm.csv":
    #         #     data = file.data
    #         #     csv = str(data)[2:-1]
    #         #     print(csv[0:5000])

    #         #String to dataframe format
    #         # csvData = csv.split('\n')
    #         # for i in range(0,len(csvData)):
    #         #     csvData[i] = csvData[i].split(",")
    #         # headers = csvData[0]
    #         # csvData = csvData[1:-1]


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
    DF1 = DF1[['%MISS', 'GETS', 'SLOW', 'NONREC', 'REC', 'TIER2', 'TIER3', '%UTIL', 'AVER_HTM', '_raw_spin_lock', 'ctx_sched_in', 'delay_mwaitx', 'THREADS', 'SLEEP', 'SPIN_COUNT']]
    DF1 = DF1.rename(columns = {'_raw_spin_lock' : 'RAW_SPIN_LOCK', 'ctx_sched_in' : 'CTX_SWITCH', 'delay_mwaitx' : 'DELAY_MWAITX'})
    DF_train = DF1[['GETS', 'SPIN_COUNT', 'NONREC', '%UTIL', 'AVER_HTM', 'RAW_SPIN_LOCK', 'CTX_SWITCH', 'DELAY_MWAITX']]
    
    #THIS IS ADDED BY ME
    DF_trainBeforeDrop = DF_train
    #DF_train = DF_train.drop(['PATTERN-NAME'], axis=1)
    
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
    print("\n\n")
    cluster_mappings = {
        0 : "Type 2 contention",
        1 : "Type 1 contention",
        2 : "low contention"
    }
    print(f"Your Java program is experiencing {cluster_mappings[pca_DF_train['Cluster'].iloc[0]]}.")

    #results = cluster_mappings[pca_DF_train['Cluster'].iloc[0]].encode()
    # resultsFile = File(filename="results.txt", data=results)
    # fileDB.session.add(resultsFile)
    # fileDB.session.commit()

    #Store results in file server
    results = cluster_mappings[pca_DF_train['Cluster'].iloc[0]]
    with open(f"{filesPath}results.txt", "w") as resultsFile:
        resultsFile.write(results)
    r = requests.post('http://localhost:5001/cds/storeInput', files={'file': ('results.txt', open(f"{filesPath}results.txt", 'rb'))})

    produce('classifierBackToCoordinator', {'fromClassifier': 'classifierComplete'})

"""---- CLASSIFIER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""



"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""

consumerClassifier = Consumer({
    'bootstrap.servers': 'localhost:9092',
    'group.id': 'module-group',
    'auto.offset.reset': 'latest'
})
consumerClassifier.subscribe(['coordinatorToClassifier'])

#---

def receipt(err, msg):
    if err is not None:
        print('Error: {}'.format(err))
    else:
        message = 'Produced message on topic {} with value of {}\n'.format(msg.topic(), msg.value().decode('utf-8'))
        print(message)

producerClassifier = Producer({'bootstrap.servers': 'localhost:9092'})
def produce(topic, message):
    data = dumps(message)
    producerClassifier.poll(1)
    producerClassifier.produce(topic, data.encode('utf-8'), callback=receipt)
    producerClassifier.flush()

"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""


while True:
    error = False
    msg=consumerClassifier.poll(1.0) #timeout
    if msg is None:
        continue
    if msg.error():
        print('Error: {}'.format(msg.error()))
        continue
    if msg.topic() == "coordinatorToClassifier":
        try:
            getFiles()
        except Exception:
            produce('classifierBackToCoordinator', {'fromClassifier': 'fileProcessingError'})
            error = True

        if not error: 
            try:
                processData()
            except Exception:
                produce('classifierBackToCoordinator', {'fromClassifier': 'classificationError'})
consumerListener.close()
            



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