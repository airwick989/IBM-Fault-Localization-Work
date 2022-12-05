from pickle import TRUE
from flask import Flask, request
from flask_cors import CORS;
import json
from werkzeug.utils import secure_filename
import os
from flask_sqlalchemy import SQLAlchemy
import pandas as pd
import numpy as np

app = Flask(__name__)
CORS(app)

#initialise variables to be used as dataframes
jlm = None
perf = None
test = None

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""

app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///../../../files.db'
fileDB = SQLAlchemy(app)

#Creates a database model, ie, a table. We will refer to this table as 'File'
class File(fileDB.Model):
    filename = fileDB.Column(fileDB.String(50) , primary_key = True)
    data = fileDB.Column(fileDB.LargeBinary)

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""

@app.route("/")
def hello():
    getFiles()
    
    return "Hello World!"


def getFiles():
    readfiles = fileDB.session.query(File)

    for file in readfiles:
        
        # Read binary data and convert it into a csv format
        data = file.data
        csv = str(data)[2:-1]
        csv = csv.replace("\\r\\n", "\n")   #'\r\n' if windows, just '\n' if linux?
        
        print(csv, file=open(f'./Files/{file.filename}', 'w', encoding='utf-8-sig'))
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


if __name__ == '__main__':
    app.run(port=5001, debug=True)