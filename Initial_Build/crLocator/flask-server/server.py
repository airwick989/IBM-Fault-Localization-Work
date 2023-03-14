from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from flask_sqlalchemy import SQLAlchemy
from json import loads, dumps
from kafka import KafkaConsumer, KafkaProducer
import os, time, re
from threading import Thread

app = Flask(__name__)
CORS(app)





"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""

app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///../../../files.db'
fileDB = SQLAlchemy(app)

#Creates a database model, ie, a table. We will refer to this table as 'File'
class File(fileDB.Model):
    filename = fileDB.Column(fileDB.String(50) , primary_key = True)
    data = fileDB.Column(fileDB.LargeBinary)

"""---- DATABASE CONFIGURATION ----------------------------------------------------------------------------------------------------------"""



"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""

def deserialize(message):
    try:
        return loads(message.decode('utf-8'))
    except Exception:
        return "Error: Message is not JSON Deserializable"

consumer = KafkaConsumer(
    'coordinatorToLocalizer',
    bootstrap_servers = ['localhost:9092'],
    auto_offset_reset = 'latest',
    enable_auto_commit = True,
    group_id = None,
    value_deserializer = deserialize
)

#---

producer = KafkaProducer(
    bootstrap_servers = ['localhost:9092'],
    value_serializer = lambda x:dumps(x).encode('utf-8')
)

"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""





"""---- LOCALIZER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""

def localize():
    #Clearing out the log directory
    if len(os.listdir('./logs/')) == 0:
        pass
    else:    
        for file in os.listdir('./logs/'):
            os.remove(f'./logs/{file}')


    filename = "./Files/Hot_1.jar"
    args = "4 100"

    start_time = "15"
    recording_length = "20"

    delay = 1
    script_running_time = delay + int(start_time) + int(recording_length) + delay

    def run_rtdriver():
        time.sleep(delay)
        os.system(f"./run_rtdriver.sh {start_time} {recording_length}")

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
    with open(f'./logs/{log_rt_file}') as file:
        flag = False
        for line in file:
            if line.split() == ['LV', 'EVENT', 'NAME']:
                flag = True

            if flag:
                if line.split()[0] == '1':
                    methods.append(line.split()[2])

    methods = list(set(methods))
    print("\n\n")
    methods_str = "The method(s) causing contention in your Java program are: "
    for method in methods:
        methods_str = methods_str + method + ", "
    print(methods_str)

"""---- LOCALIZER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""





for message in consumer:
    data = message.value
    if 'signal' in data:
        if data['signal'] == 'startLocalizer':
            print("hello there")