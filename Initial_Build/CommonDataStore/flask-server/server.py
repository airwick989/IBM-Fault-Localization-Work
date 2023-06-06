from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from json import dumps, loads
from threading import Thread
from confluent_kafka import Consumer, Producer
import os

app = Flask(__name__)
CORS(app)
app.app_context().push()  

uploadsDirectory = "Uploads/"

"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""

#'http://localhost:5000/cds/storeInput'

@app.route("/cds/storeInput", methods=['POST'])
def storeInput():
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


if __name__ == "__main__":    
    app.run(port=5001, debug=True)