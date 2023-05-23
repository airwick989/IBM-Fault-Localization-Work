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
app.app_context().push()    #added to mitigate "working outside of application context" error




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

#GET CONSUMER/PRODUCER STUFF FROM OTHER MODULES

"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""





"""---- LOCALIZER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""

def localize():
    #Clearing out the logs directory
    if len(os.listdir('./logs/')) == 0:
        pass
    else:    
        for file in os.listdir('./logs/'):
            os.remove(f'./logs/{file}')

    #Clearing out the Files directory
    if len(os.listdir('./Files/')) > 0:
        for file in os.listdir('./Files/'):
            os.remove(f'./Files/{file}') 

    jarFile = fileDB.session.query(File).filter(File.filename.like('%.jar')).first()    #another option, instead of .first(), use .all()
    print(jarFile.filename)

    filename = "./Files/{jarFile.filename}"
    # args = "4 100"

    # start_time = "15"
    # recording_length = "20"

    # delay = 1
    # script_running_time = delay + int(start_time) + int(recording_length) + delay

    # def run_rtdriver():
    #     time.sleep(delay)
    #     os.system(f"./run_rtdriver.sh {start_time} {recording_length}")

    # def run_jlm():
    #     os.system(f"./run_jlm.sh {script_running_time} {filename} {args}")

    # rtdriver = Thread(target= run_rtdriver)
    # jlm = Thread(target=run_jlm)
    # rtdriver.start()
    # jlm.start()

    # time.sleep(script_running_time)
    # rtdriver.join()
    # jlm.join()


    # #Returning the method causing lock contention
    # methods = []
    # r = re.compile("^log-rt")
    # log_rt_file = list(filter(r.match, os.listdir('./logs/')))[0]
    # prevLine = ""
    # with open(f'./logs/{log_rt_file}') as file:
    #     flag = False
    #     for line in file:
    #         if line.split() == ['LV', 'EVENT', 'NAME']:
    #             flag = True

    #         if flag:
    #             if line.split()[1] not in ['0', 'EVENT']:
    #                 # print(line.split())
    #                 # print(prevLine)
    #                 # print(prevLine.split())
    #                 methods.append(prevLine.split()[2])

    #         prevLine = line

    # methods = list(set(methods))
    # print("\n\n")
    # methods_str = "The method(s) causing contention in your Java program are: "
    # for method in methods:
    #     methods_str = methods_str + method + ", "
    # print(methods_str)

"""---- LOCALIZER FUNCTIONS ------------------------------------------------------------------------------------------------------------"""




time_threshold = 5  #message time delta threshold of 5 seconds
checkpoint_time = 0
for message in consumer:
    if time.time() - checkpoint_time > time_threshold:
        data = message.value
        if 'signal' in data:
            if data['signal'] == 'startLocalizer':
                localize()
        
    checkpoint_time = time.time()
    