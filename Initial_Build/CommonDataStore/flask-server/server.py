from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from json import dumps, loads
from threading import Thread
from confluent_kafka import Consumer, Producer
import os

app = Flask(__name__)
CORS(app)


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""



@app.route("/cds/storeInput", methods=['GET', 'POST'])
def upload():

    accepted_filenames = ['jlm.csv', 'perf.csv', 'test.csv']


if __name__ == "__main__":    
    app.run(debug=True)