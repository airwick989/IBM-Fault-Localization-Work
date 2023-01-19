from flask import Flask
from flask_cors import CORS;
from flask_sqlalchemy import SQLAlchemy
import pandas as pd
from sklearn.cluster import KMeans
from sklearn.decomposition import PCA
from sklearn.preprocessing import StandardScaler
from sklearn.preprocessing import MinMaxScaler
import matplotlib.pyplot as plt
import seaborn as sns
import pickle
import numpy as np

app = Flask(__name__)
CORS(app)
app.app_context().push()    #added to mitigate "working outside of application context" error

#initialise variables to be used as dataframes
jlm = None
perf = None
test = None

filesPath = './Files/Uploads/'
savePath = './Files/'
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


def scale_standard(df):
    #scaler = StandardScaler()
    #df_scaled = scaler.fit_transform(df)
    #scaler.fit(df)
    with open(f'{modelPath}scaler.pkl', 'rb') as f:
        scaler = pickle.load(f)
    df_scaled = scaler.transform(df)
    return df_scaled

def single_scale_standard(df):
    scaler = StandardScaler()
    df = np.array(df.values[0])
    df_scaled = scaler.fit_transform(df[:, np.newaxis])
    df_scaled_list = []
    for value in df_scaled:
        df_scaled_list.append(value[0])
    return df_scaled_list


def scale_minmax(df):
    scaler = MinMaxScaler()
    df_scaled = scaler.fit_transform(df)
    return df_scaled


def apply_pca(df):
    #pca = PCA(n_components=2)
    # features = pca.fit_transform(df)
    # pca.fit(df)
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

    #classifyScatterPlot(pca_DF_train, DF_trainBeforeDrop)
    classify(pca_DF_train, DF_trainBeforeDrop)


def classifyScatterPlot(pca_DF_train, DF_train):
    df = pd.DataFrame({'x': pca_DF_train['pc1'], 'y': pca_DF_train['pc2'], 'z': DF_train['PATTERN-NAME']})

    # df['Cluster'] = kmeans12.labels_
    # plt.figure(figsize = (6,6))
    # plt.style.use("seaborn")
    # sns.color_palette("Paired")
    # groups = df.groupby('Cluster')
    # for name, group in groups:
    #     plt.plot(group.x, group.y, marker='o', linestyle='', markersize=3, label=name)
    # plt.legend()
    # plt.show() #This is the resultant plot, commented out for now


def classify(pca_DF_train, DF_train):
    #kmeans12 = KMeans(n_clusters=3, init='k-means++', max_iter=600, n_init=10)
    #kmeans12.fit(pca_DF_train.values)
    
    #df = pd.DataFrame({'x': pca_DF_train['pc1'], 'y': pca_DF_train['pc2'], 'z': DF_train['PATTERN-NAME']})
    #print(df)
    with open(f'{modelPath}kmeans12.pkl', 'rb') as f:
        kmeans12 = pickle.load(f)

    predicted_clusters = []
    for index, row in pca_DF_train.iterrows():
        #print(kmeans12.predict([[row['pc1'], row['pc2']]])[0])
        predicted_clusters.append(kmeans12.predict([[row['pc1'], row['pc2']]])[0])
    pca_DF_train['Cluster'] = predicted_clusters
    print(pca_DF_train)
    
    #print(kmeans12.predict([[-2.596045, 1.272793]]))



"""---- CLASSIFIER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""




#getFiles()
processData()