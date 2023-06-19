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
    

    
    
@app.route("/cds/getData", methods=['GET'])
def getData():

    targetExtensions = request.args.get('targetExtensions')
    targetExtensions = targetExtensions.split(',')
    #files = []
    with zipfile.ZipFile(f'{uploadsDirectory}files.zip', 'w') as zip:
        for file in os.listdir(uploadsDirectory):
            for extension in targetExtensions:
                extension = extension.strip()
                if file.endswith(extension):
                    #files.append(('file', open(f"{uploadsDirectory}{file}", 'rb')))
                    zip.write(f"{uploadsDirectory}{file}", compress_type=zipfile.ZIP_DEFLATED)

    return send_file(f"{uploadsDirectory}files.zip")
    #return "ok"


if __name__ == "__main__":    
    app.run(port=5001, debug=True)