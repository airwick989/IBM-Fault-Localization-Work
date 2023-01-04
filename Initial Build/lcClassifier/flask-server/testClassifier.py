from flask import Flask
from flask_cors import CORS;
from flask_sqlalchemy import SQLAlchemy
import pandas as pd
from sklearn.cluster import KMeans
from sklearn.decomposition import PCA
from sklearn.preprocessing import StandardScaler
from sklearn.preprocessing import MinMaxScaler

app = Flask(__name__)
CORS(app)
app.app_context().push()    #added to mitigate "working outside of application context" error

#initialise variables to be used as dataframes
jlm = None
perf = None
test = None

filesPath = './Files/'


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

    kmeans13 = KMeans(n_clusters=3, init='k-means++', max_iter=600, n_init=10)
    kmeans13.fit(pca_DF_train)
    DF1['CLUSTER_TYPE'] = pd.Series(kmeans13.labels_, index=DF1.index)

    classify(DF1)

def classify(DF):
    df_box = DF.copy()
    df_box = df_box.drop(['THREADS', 'SLEEP', '%UTIL'], axis=1)

    type_zero_total = 0
    type_zero_count = 0

    type_one_total = 0
    type_one_count = 0

    type_two_total = 0
    type_two_count = 0

    for row in df_box.index:
        #print(df_box['AVER_HTM'][row], df_box['CLUSTER_TYPE'][row])
        if df_box['CLUSTER_TYPE'][row] == 0:
            type_zero_count += 1
            type_zero_total += df_box['AVER_HTM'][row]
        elif df_box['CLUSTER_TYPE'][row] == 1:
            type_one_count += 1
            type_one_total += df_box['AVER_HTM'][row]
        else:
            type_two_count += 1
            type_two_total += df_box['AVER_HTM'][row]
    
    type_zero_mean = type_zero_total/type_zero_count
    type_one_mean = type_one_total/type_one_count
    type_two_mean = type_two_total/type_two_count

    valueToTypeDict = {
        'Type 0': type_zero_mean,
        'Type 1': type_one_mean,
        'Type 2': type_two_mean
    }

    print(list(valueToTypeDict.keys())[list(valueToTypeDict.values()).index(max([type_zero_mean, type_one_mean, type_two_mean]))])

"""---- CLASSIFIER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""




getFiles()
processData()