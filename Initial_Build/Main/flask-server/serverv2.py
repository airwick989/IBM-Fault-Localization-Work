from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from json import dumps, loads
from threading import Thread
from confluent_kafka import Consumer, Producer
import os
import requests

app = Flask(__name__)
CORS(app)

jarSaveDirectory = '../../jarFile/'
inputSavePath = 'Uploads/'  #/Uploads/ caused a ridiculous wsgi error, something not iterable or whatever


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""

producerCoordinator = Producer({'bootstrap.servers': 'localhost:9092'})

def receipt(err, msg):
    if err is not None:
        print('Error: {}'.format(err))
    else:
        message = 'Produced message on topic {} with value of {}\n'.format(msg.topic(), msg.value().decode('utf-8'))
        print(message)

def produce(topic, message):
    data = dumps(message)
    producerCoordinator.poll(1)
    producerCoordinator.produce(topic, data.encode('utf-8'), callback=receipt)
    producerCoordinator.flush()


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""




def string_to_file(string, savePath):
    with open(savePath, 'w') as file:
        file.write(string)


@app.route("/upload", methods=['GET', 'POST'])
def upload():

    accepted_filenames = ['jlm.csv', 'perf.csv', 'test.csv']

    try:
        files = request.files
        filelist = files.getlist("file")
        #files = jsonpickle.encode(files)
        flag = True
        javaProgramArgs = request.form.get('args')
        start_time = request.form.get('start_time')
        recording_length = request.form.get('recording_length')
        errorType = ""
        csvCount = 0
        jarCount = 0

        #Preliminary Check
        for f in filelist:
            filename = secure_filename(f.filename)
            
            if filename not in accepted_filenames and not filename.endswith(".jar"):
                flag = False
                errorType = "FileNameError"
                break
            if filename.endswith(".csv"):
                csvCount += 1
            else:
                jarCount += 1
        if csvCount != 3 or jarCount != 1:
            flag = False
            errorType = "FileCountError"


        if flag == True:

            #Clear Uploads directory
            if len(os.listdir(inputSavePath)) > 0:
                for file in os.listdir(inputSavePath):
                    os.remove(f'{inputSavePath}{file}')

            #Store Application args if there are any
            if javaProgramArgs != "":
                string_to_file(javaProgramArgs, f"{inputSavePath}javaProgramArgs.args")

            #Store localization parameters
            string_to_file(f"{start_time} {recording_length}", f"{inputSavePath}localizationParams.params")

            for f in filelist:
                filename = secure_filename(f.filename)
                f.save(f"{inputSavePath}{filename}")

            files = []
            for file in os.listdir(inputSavePath):
                files.append(('file', open(f"{inputSavePath}{file}", 'rb')))
            headers = {
                'clear_flag': 'True'
            }
            saveInputReq = requests.post('http://localhost:5001/cds/storeData', files=files, headers=headers)

            produce('coordinatorToClassifier', {'fromCoordinator': 'startClassifier'})

            return "ok"
        else:
            return errorType
    except Exception:
        return Exception
    



@app.route("/loading", methods=['GET'])
def loading():

    #Consumer to aid loading screen
    consumerLoading = Consumer({
        'bootstrap.servers': 'localhost:9092',
        'group.id': 'middleware-group',
        'auto.offset.reset': 'latest'
    })
    consumerLoading.subscribe(['middlewareNotifier'])

    loadingFlag = True
    success = None
    data = None

    while loadingFlag:
        msg=consumerLoading.poll(1.0) #timeout
        if msg is None:
            continue
        if msg.error():
            print('Error: {}'.format(msg.error()))
            continue
        if msg.topic() == "middlewareNotifier":
            data = loads(msg.value().decode('utf-8'))
            if data["fromLocalizer"] == "localizerComplete":
                success = True
            else:
                success = False
            loadingFlag = False

    consumerLoading.close()

    if success:
        print("DOG")
        return "completed"
    else:
        return f"ERROR in Localizer: {data['fromLocalizer']}"




if __name__ == "__main__":
    
    app.run(debug=True)