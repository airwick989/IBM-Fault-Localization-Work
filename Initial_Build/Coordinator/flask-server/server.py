from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from flask_sqlalchemy import SQLAlchemy
from time import sleep
from json import dumps, loads
from kafka import KafkaProducer, KafkaConsumer
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


"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""

producer = KafkaProducer(
    bootstrap_servers = ['localhost:9092'],
    value_serializer = lambda x:dumps(x).encode('utf-8')
)

def initiate_event(producer, topic):
    message = {'signal': "start"}
    producer.send(topic, value=message)

#---

def deserialize(message):
    try:
        return loads(message.decode('utf-8'))
    except Exception:
        return "Error: Message is not JSON Deserializable"

consumerClassifier = KafkaConsumer(
    'classifierBackToCoordinator',
    bootstrap_servers = ['localhost:9092'],
    auto_offset_reset = 'latest',
    enable_auto_commit = True,
    group_id = None,
    value_deserializer = deserialize
)

def listen(consumer, producer, producerTopic):
    for message in consumer:
        data = message.value
        if 'signal' in data:
            if data['signal'] == 'start':
                initiate_event(producer, producerTopic)

"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""


@app.route("/upload", methods=['GET', 'POST'])
def upload():

    accepted_filenames = ['jlm.csv', 'perf.csv', 'test.csv']

    try:
        files = request.files.getlist("file")
        flag = True
        errorType = ""
        csvCount = 0
        javaCount = 0

        #Preliminary Check
        for f in files:
            filename = secure_filename(f.filename)
            
            if filename not in accepted_filenames and not filename.endswith(".java"):
                flag = False
                errorType = "FileNameError"
                break
            if filename.endswith(".csv"):
                csvCount += 1
            else:
                javaCount += 1
        if csvCount != 3 or javaCount != 1:
            flag = False
            errorType = "FileCountError"


        if flag == True:
            for f in files:
                filename = secure_filename(f.filename)

                data = f.read()
                exists = bool(fileDB.session.query(File).filter_by(filename=filename).first())
                if exists:
                    file = fileDB.session.query(File).filter(File.filename == filename).one()
                    file.data = data
                    fileDB.session.commit()
                else:
                    file = File(filename=filename, data=data) #Create a 'File' object of the File class in the DATABASE CONFIGURATION part of the code
                    fileDB.session.add(file)
                    fileDB.session.commit()

                #Save locally (for testing purposes)
                #f.save(os.getcwd() + '\\Uploads\\' + filename)

            initiate_event(producer, 'coordinatorToClassifier')

            return "ok"
        else:
            return errorType
    except Exception:
        return Exception

if __name__ == "__main__":
    classifierListener = Thread(target= listen, args=[consumerClassifier, producer, 'coordinatorToLocalizer'])
    classifierListener.start()
    app.run(debug=True)