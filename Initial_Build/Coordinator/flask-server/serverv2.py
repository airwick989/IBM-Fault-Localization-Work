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


"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""

#FOLLOWING CODE WAS COMMENTED OUT FOR FILE SERVER IMPLEMENTATION

# app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
# app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///../../../files.db'
# fileDB = SQLAlchemy(app)

# #Creates a database model, ie, a table. We will refer to this table as 'File'
# class File(fileDB.Model):
#     filename = fileDB.Column(fileDB.String(50) , primary_key = True)
#     data = fileDB.Column(fileDB.LargeBinary)

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""


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

def listen():
    consumerListener = Consumer({
        'bootstrap.servers': 'localhost:9092',
        'group.id': 'coordinator-group',
        'auto.offset.reset': 'latest'
    })
    consumerListener.subscribe(['classifierBackToCoordinator', 'localizerBackToCoordinator'])

    while True:
        msg=consumerListener.poll(1.0) #timeout
        if msg is None:
            continue
        if msg.error():
            print('Error: {}'.format(msg.error()))
            continue
        # data=msg.value().decode('utf-8')
        # print(data)
        if msg.topic() == "classifierBackToCoordinator":
            data = loads(msg.value().decode('utf-8'))
            if data["fromClassifier"] == "classifierComplete":
                produce('coordinatorToLocalizer', {'fromCoordinator': 'startLocalizer'})
            else:
                print(f"ERROR in Classifier: {data['fromClassifier']}")
        elif msg.topic() == "localizerBackToCoordinator":
            data = loads(msg.value().decode('utf-8'))
            if data["fromLocalizer"] == "localizerComplete":
                produce('coordinatorToPatternMatcher', {'fromCoordinator': 'startPatternMatching'})
            else:
                print(f"ERROR in Localizer: {data['fromLocalizer']}")
    consumerListener.close()


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

            # #The following 2 lines replace the block of commented out code below
            # fileDB.session.query(File).delete()
            # fileDB.session.commit()

            # if javaProgramArgs != "":
            #     javaProgramArgs = javaProgramArgs.encode()
            #     argsFile = File(filename="javaProgramArgs.txt", data=javaProgramArgs)
            #     fileDB.session.add(argsFile)
            #     fileDB.session.commit()

            # localizationParams = f"{start_time} {recording_length}"
            # localizationParams = localizationParams.encode()
            # localizationParams = File(filename="localizationParams.txt", data=localizationParams)
            # fileDB.session.add(localizationParams)
            # fileDB.session.commit()

            #ABOVE CODE WAS COMMENTED OUT FOR FILE SERVER IMPLEMENTATION

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
                #EVERYTHING BELOW WAS COMMENTED OUT FOR FILE SERVER IMPLEMENTATION
                
                # #added if statement and put rest in else block for localization
                # if filename.endswith(".jar"):
                #     #Clearing out the jarFile directory
                #     if len(os.listdir(jarSaveDirectory)) > 0:
                #         for file in os.listdir(jarSaveDirectory):
                #             os.remove(f'{jarSaveDirectory}{file}') 
                #     f.save(f"{jarSaveDirectory}{filename}")
                # else:
                #     data = f.read()
                #     # exists = bool(fileDB.session.query(File).filter_by(filename=filename).first())
                #     # if filename.endswith(".jar") and fileDB.session.query(File).filter(File.filename.like('%.jar')).count() == 1:
                #     #     fileDB.session.query(File).filter(File.filename.like('%.jar')).delete()
                #     # if exists:
                #     #     file = fileDB.session.query(File).filter(File.filename == filename).one()
                #     #     file.data = data
                #     #     fileDB.session.commit()
                #     # else:
                #     file = File(filename=filename, data=data) #Create a 'File' object of the File class in the DATABASE CONFIGURATION part of the code
                #     fileDB.session.add(file)
                #     fileDB.session.commit()

                #     #Save locally (for testing purposes)
                #     #f.save(os.getcwd() + '\\Uploads\\' + filename)

            files = []
            for file in os.listdir(inputSavePath):
                files.append(('file', open(f"{inputSavePath}{file}", 'rb')))
            headers = {
                'clear_flag': 'True'
            }
            saveInputReq = requests.post('http://localhost:5001/cds/storeData', files=files, headers=headers)

            #producer.send('coordinatorToClassifier', value={'signal': "startClassifier"})
            produce('coordinatorToClassifier', {'fromCoordinator': 'startClassifier'})

            return "ok"
        else:
            return errorType
    except Exception:
        return Exception
    



# @app.route("/loading", methods=['GET'])
# def loading():

#     #Consumer to aid loading screens
#     consumerLoading = Consumer({
#         'bootstrap.servers': 'localhost:9092',
#         'group.id': 'coordinator-group',
#         'auto.offset.reset': 'latest'
#     })
#     consumerLoading.subscribe(['localizerBackToCoordinator'])

#     loadingFlag = True
#     success = None
#     data = None

#     while loadingFlag:
#         msg=consumerLoading.poll(1.0) #timeout
#         if msg is None:
#             continue
#         if msg.error():
#             print('Error: {}'.format(msg.error()))
#             continue
#         if msg.topic() == "localizerBackToCoordinator":
#             data = loads(msg.value().decode('utf-8'))
#             if data["fromLocalizer"] == "localizerComplete":
#                 success = True
#             else:
#                 success = False
#             loadingFlag = False

#     consumerLoading.close()

#     if success:
#         return "completed"
#     else:
#         return f"ERROR in Localizer: {data['fromLocalizer']}"




if __name__ == "__main__":
    # listener = Thread(target= listen, args=[consumerClassifier, "classifierDone", producer, 'coordinatorToLocalizer', {'signal': "startLocalizer"}])
    # listener.start()

    listener = Thread(target= listen)
    listener.start()
    
    app.run(debug=True)