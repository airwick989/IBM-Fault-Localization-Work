from pickle import TRUE
from flask import Flask, request
from flask_cors import CORS;
import json
from werkzeug.utils import secure_filename
import os
from flask_sqlalchemy import SQLAlchemy

app = Flask(__name__)
CORS(app)

@app.route("/")
def hello():
    return "Hello World!"

if __name__ == '__main__':
    app.run(port=5001, debug=True)