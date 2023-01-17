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

app = Flask(__name__)
CORS(app)
app.app_context().push()    #added to mitigate "working outside of application context" error

#initialise variables to be used as dataframes
jlm = None
perf = None
test = None

filesPath = './Files/combined_training_data/'
savePath = './Files/'


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
    scaler = StandardScaler()
    df_scaled = scaler.fit_transform(df)
    return df_scaled


def scale_minmax(df):
    scaler = MinMaxScaler()
    df_scaled = scaler.fit_transform(df)
    return df_scaled


def apply_pca(df):
    pca = PCA(n_components=2)
    features = pca.fit_transform(df)
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
    pca_DF_train = pd.DataFrame(data = apply_pca(DF_train), columns = ['pc1', 'pc2'])

    df = pd.DataFrame({'x': pca_DF_train['pc1'], 'y': pca_DF_train['pc2'], 'z': DF_trainBeforeDrop['PATTERN-NAME']})

    classifyScatterPlot(pca_DF_train, DF_trainBeforeDrop)
    #classify(pca_DF_train, DF_train)


def classifyScatterPlot(pca_DF_train, DF_train):
    df = pd.DataFrame({'x': pca_DF_train['pc1'], 'y': pca_DF_train['pc2'], 'z': DF_train['PATTERN-NAME']})
    with open(f'{savePath}kmeans12.pkl', 'rb') as f:
        kmeans12 = pickle.load(f)
    df['Cluster'] = kmeans12.labels_

    plt.figure(figsize = (6,6))
    plt.style.use("seaborn")
    sns.color_palette("Paired")
    groups = df.groupby('Cluster')
    for name, group in groups:
        plt.plot(group.x, group.y, marker='o', linestyle='', markersize=3, label=name)
    plt.legend()
    plt.show() #This is the resultant plot, commented out for now


def classify(pca_DF_train, DF_train):
    #kmeans12 = KMeans(n_clusters=3, init='k-means++', max_iter=600, n_init=10)
    #kmeans12.fit(pca_DF_train.values)
    
    with open(f'{savePath}kmeans12.pkl', 'rb') as f:
        kmeans12 = pickle.load(f)

    # for i in range(0,100):
    #     print(kmeans12.predict([[-1.22576610990847, -0.304350779950182]]))



"""---- CLASSIFIER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""




getFiles()
processData()