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
        print(file.filename)
        if file.filename == 'jlm.csv':
            data = file.data
            print(data)


if __name__ == '__main__':
    app.run(port=5001, debug=True)