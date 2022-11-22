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

    accepted_filenames = ['jlm.csv', 'perf.csv', 'test.csv']

    try:
        files = request.files.getlist("file")
        flag = True
        for f in files:
            filename = secure_filename(f.filename)
            if filename not in accepted_filenames:
                flag = False
                break
            f.save(os.getcwd() + '\\Uploads\\' + filename)

        if flag == True:
            return "ok"
        else:
            return "fileNameError"
            
    except Exception:
        return Exception

if __name__ == "__main__":
    app.run(debug=True)