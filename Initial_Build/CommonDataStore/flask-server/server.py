from flask import Flask, request, send_file
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from json import dumps, loads
from threading import Thread
from confluent_kafka import Consumer, Producer
import os
import zipfile

app = Flask(__name__)
CORS(app)
app.app_context().push()  

uploadsDirectory = "Uploads/"

"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""

#'http://localhost:5001/cds/storeInput'

@app.route("/cds/storeData", methods=['POST'])
def storeData():
    try:
        if 'clear_flag' in request.headers:
            if request.headers['clear_flag'] == 'True':
                #Clear Uploads directory
                if len(os.listdir(uploadsDirectory)) > 0:
                    for file in os.listdir(uploadsDirectory):
                        os.remove(f'{uploadsDirectory}{file}')
        
        files = request.files
        filelist = files.getlist("file")
        for f in filelist:
            filename = secure_filename(f.filename)
            f.save(f"{uploadsDirectory}{filename}")
        return "ok"
    except Exception:
        return Exception
    

    

def zipFiles(targetExtensions):
    #files = []
    with zipfile.ZipFile(f'{uploadsDirectory}files.zip', 'w') as zip:
        for file in os.listdir(uploadsDirectory):
            for extension in targetExtensions:
                extension = extension.strip()
                if file.endswith(extension):
                    #files.append(('file', open(f"{uploadsDirectory}{file}", 'rb')))
                    zip.write(f"{uploadsDirectory}{file}", compress_type=zipfile.ZIP_DEFLATED)




@app.route("/cds/getData", methods=['GET'])
def getData():
    target = request.args.get('target')
    target = target.split(',')
    isMultiple = request.args.get('isMultiple')
    print(isMultiple)
    if isMultiple == 'true':
        zipFiles(target)
        return send_file(f"{uploadsDirectory}files.zip")
    else:
        filename = target[0].strip()
        return send_file(f"{uploadsDirectory}{filename}")
    #return "ok"




@app.route("/cds/interResults", methods=['GET'])
def interResults():
    
    results = []
    with open(f'{uploadsDirectory}results.results', 'r') as results_file:
        results = results_file.read().strip()
        results = results.split("\n")

    try:
        methods = results[1][:-1].split(", ")
    except Exception:
        methods = "None"

    data = {
        'lctype': results[0],
        'methods': methods
    }

    data = dumps(data)

    return data




if __name__ == "__main__":    
    app.run(port=5001, debug=True)