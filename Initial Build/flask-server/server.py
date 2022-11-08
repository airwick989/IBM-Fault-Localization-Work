from pickle import TRUE
from flask import Flask, request
from flask_cors import CORS;
import json
from werkzeug.utils import secure_filename
import os

app = Flask(__name__)
CORS(app)

@app.route("/upload", methods=['GET', 'POST'])
def upload():
    files = request.files.getlist("file")
    for f in files:
        filename = secure_filename(f.filename)
        f.save(os.getcwd() + '\\Uploads\\' + filename)
    return "ok"

if __name__ == "__main__":
    app.run(debug=True)