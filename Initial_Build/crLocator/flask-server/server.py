from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from flask_sqlalchemy import SQLAlchemy
from time import sleep
from json import dumps
from kafka import KafkaProducer

app = Flask(__name__)
CORS(app)