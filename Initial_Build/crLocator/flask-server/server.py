from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from flask_sqlalchemy import SQLAlchemy
from json import loads, dumps
import ast
from kafka import KafkaConsumer, KafkaProducer
import os, time, re
from threading import Thread
from confluent_kafka import Consumer, Producer
import requests
import zipfile
import shutil

app = Flask(__name__)
CORS(app)
app.app_context().push()    #added to mitigate "working outside of application context" error

filesPath = './Files/Uploads/'
zipPath = f'{filesPath}files.zip'
embeddedUploadsPath = f"{filesPath}Uploads/"

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""

# app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
# app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///../../../files.db'
# fileDB = SQLAlchemy(app)

# #Creates a database model, ie, a table. We will refer to this table as 'File'
# class File(fileDB.Model):
#     filename = fileDB.Column(fileDB.String(50) , primary_key = True)
#     data = fileDB.Column(fileDB.LargeBinary)

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""



"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""

consumerLocalizer = Consumer({
    'bootstrap.servers': 'localhost:9092',
    'group.id': 'module-group',
    'auto.offset.reset': 'latest'
})
consumerLocalizer.subscribe(['coordinatorToLocalizer'])

#---

def receipt(err, msg):
    if err is not None:
        print('Error: {}'.format(err))
    else:
        message = 'Produced message on topic {} with value of {}\n'.format(msg.topic(), msg.value().decode('utf-8'))
        print(message)

producerLocalizer = Producer({'bootstrap.servers': 'localhost:9092'})
def produce(topic, message):
    data = dumps(message)
    producerLocalizer.poll(1)
    producerLocalizer.produce(topic, data.encode('utf-8'), callback=receipt)
    producerLocalizer.flush()

"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""





"""---- LOCALIZER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""

def localize():
    #Clearing out the logs directory
    if len(os.listdir('./logs/')) == 0:
        pass
    else:    
        for file in os.listdir('./logs/'):
            os.remove(f'./logs/{file}')

    # #Pull Jar file from the common data store
    # jarFile = fileDB.session.query(File).filter(File.filename.like('%.jar')).first()    #another option, instead of .first(), use .all()
    # filename = f"./Files/{jarFile.filename}"
    # data = jarFile.data
    # print(data, file=open(filename, 'w'))

    #Clear Uploads directory
    if len(os.listdir(filesPath)) > 0:
        for file in os.listdir(filesPath):
            #Checks for uploads directory in the directory and clears it
            if os.path.isdir(f"{filesPath}{file}"):
                shutil.rmtree(embeddedUploadsPath)
            else:
                os.remove(f'{filesPath}{file}')

    #Pull necessary files from file server
    params = {
        'targetExtensions': ".jar, .args, .params, .results"
    }
    response = requests.get('http://localhost:5001/cds/getData', params=params)
    #Write the zip file
    open(zipPath, "wb").write(response.content)

    #Extract all files from zip
    with zipfile.ZipFile(zipPath, 'r') as zip:
        zip.extractall(filesPath)
    #Move files to correct directory and remove unecessary folder
    if os.path.isdir(embeddedUploadsPath):
        for file in os.listdir(embeddedUploadsPath):
            os.rename(f"{embeddedUploadsPath}{file}", f"{filesPath}{file}")
        shutil.rmtree(embeddedUploadsPath)

    #Get program arguments if they exist
    args = ""
    argsPath = f'{filesPath}javaProgramArgs.args'
    if os.path.exists(argsPath):
        with open(argsPath, 'r') as file:
            args = file.read().rstrip()

    #Get start_time and recording
    localizationParams = None
    with open(f'{filesPath}localizationParams.params', 'r') as file:
        localizationParams = file.read().rstrip()
    localizationParams = localizationParams.split()

    # start_time = "15"
    # recording_length = "20"
    start_time = localizationParams[0]
    recording_length = localizationParams[1]

    delay = 1
    script_running_time = delay + int(start_time) + int(recording_length) + delay

    def run_rtdriver():
        time.sleep(delay)
        os.system(f"./run_rtdriver.sh {start_time} {recording_length}")

    #get filename of jar file
    filename = ""
    for file in os.listdir(filesPath):
        if file.endswith(".jar"):
            filename = f"{filesPath}{file}"

    def run_jlm():
        os.system(f"./run_jlm.sh {script_running_time} {filename} {args}")

    rtdriver = Thread(target= run_rtdriver)
    jlm = Thread(target=run_jlm)
    rtdriver.start()
    jlm.start()

    time.sleep(script_running_time)
    rtdriver.join()
    jlm.join()


    #Returning the method causing lock contention
    methods = []
    r = re.compile("^log-rt")
    log_rt_file = list(filter(r.match, os.listdir('./logs/')))[0]
    prevLine = ""
    with open(f'./logs/{log_rt_file}') as file:
        flag = False
        for line in file:
            if line.split() == ['LV', 'EVENT', 'NAME']:
                flag = True

            if flag:
                if line.split()[1] not in ['0', 'EVENT']:
                    # print(line.split())
                    # print(prevLine)
                    # print(prevLine.split())
                    methods.append(prevLine.split()[2])

            prevLine = line

    methods = list(set(methods))
    print("\n\n")
    methods_str = "The method(s) causing contention in your Java program are: "
    for method in methods:
        methods_str = methods_str + method + ", "
    print(methods_str)

    produce('localizerBackToCoordinator', {'fromLocalizer': 'localizerComplete'})

"""---- LOCALIZER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""



# #THE FOLLOWING IS IMPORTANT FOR THE TIMING THRESHOLD MECHANISM FOR AVOIDING DUPLICATE MESSAGES
# time_threshold = 5  #message time delta threshold of 5 seconds
# checkpoint_time = 0
# for message in consumer:
#     if time.time() - checkpoint_time > time_threshold:
#         data = message.value
#         if 'signal' in data:
#             if data['signal'] == 'startLocalizer':
#                 localize()
        
#     checkpoint_time = time.time()




#localize()
while True:
    msg=consumerLocalizer.poll(1.0) #timeout
    if msg is None:
        continue
    if msg.error():
        print('Error: {}'.format(msg.error()))
        continue
    if msg.topic() == "coordinatorToLocalizer":
        try:
            localize()
        except Exception:
            produce('localizerBackToCoordinator', {'fromLocalizer': 'localizationError'})
consumerLocalizer.close()