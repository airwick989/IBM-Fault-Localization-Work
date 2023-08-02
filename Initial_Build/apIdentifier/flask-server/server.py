from flask import Flask
from flask_cors import CORS;
from json import loads, dumps
import os
from threading import Thread
from confluent_kafka import Consumer, Producer
import shutil
import zipfile
import subprocess
import re
import json
import requests

app = Flask(__name__)
CORS(app)
app.app_context().push()    #added to mitigate "working outside of application context" error

filesPath = './Files/'
zipPath = f'{filesPath}files.zip'
uploadsPath = f"{filesPath}Uploads/"
pmDirectory = './staticAnalysis-main/maven/my-app/'
inputDirectory = f'{pmDirectory}input/'



"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""

producerPatternMatcher = Producer({'bootstrap.servers': 'localhost:9092'})

consumerPatternMatcher = Consumer({
    'bootstrap.servers': 'localhost:9092',
    'group.id': 'module-group',
    'auto.offset.reset': 'latest'
})
consumerPatternMatcher.subscribe(['coordinatorToPatternMatcher'])

#---

def receipt(err, msg):
    if err is not None:
        print('Error: {}'.format(err))
    else:
        message = 'Produced message on topic {} with value of {}\n'.format(msg.topic(), msg.value().decode('utf-8'))
        print(message)

def produce(topic, message):
    data = dumps(message)
    producerPatternMatcher.poll(1)
    producerPatternMatcher.produce(topic, data.encode('utf-8'), callback=receipt)
    producerPatternMatcher.flush()

"""---- KAFKA CONSUMER / PRODUCER ------------------------------------------------------------------------------------------------------------------"""




"""---- PATTERN MATCHER FUNCTIONS ------------------------------------------------------------------------------------------------------------------"""

def identifyAP():
    
    #Clear Uploads directory
    if len(os.listdir(filesPath)) > 0:
        for file in os.listdir(filesPath):
            #Checks for uploads directory in the directory and clears it
            if os.path.isdir(f"{filesPath}{file}"):
                shutil.rmtree(uploadsPath)
            else:
                os.remove(f'{filesPath}{file}')

    #Clear Input directory
    if len(os.listdir(inputDirectory)) > 0:
        for file in os.listdir(inputDirectory):
            os.remove(f'{inputDirectory}{file}')

    #Pull necessary files from file server
    params = {
        'target': ".java",
        'isMultiple': 'true'
    }
    response = requests.get('http://localhost:5001/cds/getData', params=params)
    #Write the zip file
    open(zipPath, "wb").write(response.content)

    #Extract all files from zip
    with zipfile.ZipFile(zipPath, 'r') as zip:
        zip.extractall(filesPath)
    #Move files to correct directory and remove unecessary folder
    if os.path.isdir(uploadsPath):
        for file in os.listdir(uploadsPath):
            os.rename(f"{uploadsPath}{file}", f"{inputDirectory}{file}")
        shutil.rmtree(uploadsPath)

    
        #Execute pattern matching
        os.chdir(pmDirectory)
        output = subprocess.run('java -cp "target/dependency/*:target/my-app-1.0-SNAPSHOT.jar" com.mycompany.app.App false false null null', capture_output=True, shell=True).stdout
        output = output.decode("utf-8")
        os.chdir("../../../")

        APs = list(filter(None, output[-14:].split(" ")))
        anti_patterns = {
            'Hot1': APs[0],
            'Hot2': APs[1],
            'Hot3_1': APs[2],
            'Hot3_2': APs[3],
            'Overly Split': APs[4],
            'Simultaneous': APs[5],
            'Unpredictable': APs[6],
        }
        
        synch_regions = output[:-14]
        synch_regions = re.split('\d+/\d+', synch_regions)
        
        files = synch_regions[0]
        files = list(filter(None, files.split("\n")))
        for i in range(0, len(files)):
            files[i] = files[i].strip("input/")

        synch_regions = synch_regions[1:]

        apData = {
            'files': files,
            'synch_regions': synch_regions,
            'anti_patterns': anti_patterns
        }
        with open(f"{filesPath}pattern_matcher.ap", "w") as jsonfile:
            jsonfile.write(json.dumps(apData, indent=4, separators=(',',': ')))

        r = requests.post('http://localhost:5001/cds/storeData', files={'file': ('pattern_matcher.ap', open(f"{filesPath}pattern_matcher.ap", 'rb'))})

        produce('patternMatcherBackToCoordinator', {'fromPatternMatcher': 'PatternMatcherComplete'})
        produce('middlewareNotifier', {'fromPatternMatcher': 'PatternMatcherComplete'})

"""---- PATTERN MATCHER FUNCTIONS ------------------------------------------------------------------------------------------------------------------"""




#identifyAP()
while True:
    msg=consumerPatternMatcher.poll(1.0) #timeout
    if msg is None:
        continue
    if msg.error():
        print('Error: {}'.format(msg.error()))
        continue
    if msg.topic() == "coordinatorToPatternMatcher":
        try:
            identifyAP()
        except Exception:
            produce('patternMatcherBackToCoordinator', {'fromPatternMatcher': 'PatternMatcherError'})
            produce('middlewareNotifier', {'fromPatternMatcher': 'PatternMatcherError'})
consumerPatternMatcher.close()