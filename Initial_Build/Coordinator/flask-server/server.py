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
app.app_context().push()    #added to mitigate "working outside of application context" error


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""

producerCoordinator = Producer({'bootstrap.servers': 'localhost:9092'})

consumerListener = Consumer({
        'bootstrap.servers': 'localhost:9092',
        'group.id': 'coordinator-group',
        'auto.offset.reset': 'latest'
    })
consumerListener.subscribe(['classifierBackToCoordinator', 'localizerBackToCoordinator'])

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