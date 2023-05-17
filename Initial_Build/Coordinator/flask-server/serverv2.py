from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from flask_sqlalchemy import SQLAlchemy
from time import sleep
from json import dumps, loads
from kafka import KafkaProducer, KafkaConsumer
from threading import Thread
from confluent_kafka import Consumer

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

#---

def deserialize(message):
    try:
        return loads(message.decode('utf-8'))
    except Exception:
        return "Error: Message is not JSON Deserializable"

# consumerClassifier = KafkaConsumer(
#     'classifierBackToCoordinator',
#     bootstrap_servers = ['localhost:9092'],
#     auto_offset_reset = 'latest',
#     enable_auto_commit = True,
#     group_id = None,
#     value_deserializer = deserialize
# )

consumerListener = Consumer({
    'bootstrap.servers': 'localhost:9092',
    'group.id': 'coordinator-group',
    'auto.offset.reset': 'latest'
})

consumerListener.subscribe(['classifierBackToCoordinator', 'localizerBackToCoordinator'])

# def listen(consumer, signalMessage, producer, producerTopic, producerMessage):
#     for message in consumer:
#         data = message.value

#         if 'signal' in data:
#             if data['signal'] == signalMessage:
#                 producer.send(producerTopic, value=producerMessage)

"""---- KAFKA PRODUCER / CONSUMER ------------------------------------------------------------------------------------------------------------------"""



@app.route("/upload", methods=['GET', 'POST'])
def upload():

    accepted_filenames = ['jlm.csv', 'perf.csv', 'test.csv']

    try:
        files = request.files.getlist("file")
        flag = True
        javaProgramArgs = request.form.get('args')
        errorType = ""
        csvCount = 0
        jarCount = 0

        #Preliminary Check
        for f in files:
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

            #The following 2 lines replace the block of commented out code below
            fileDB.session.query(File).delete()
            fileDB.session.commit()

            if javaProgramArgs != "":
                javaProgramArgs = javaProgramArgs.encode()
                argsFile = File(filename="javaProgramArgs.txt", data=javaProgramArgs)
                fileDB.session.add(argsFile)
                fileDB.session.commit()

            for f in files:
                filename = secure_filename(f.filename)

                data = f.read()
                # exists = bool(fileDB.session.query(File).filter_by(filename=filename).first())
                # if filename.endswith(".jar") and fileDB.session.query(File).filter(File.filename.like('%.jar')).count() == 1:
                #     fileDB.session.query(File).filter(File.filename.like('%.jar')).delete()
                # if exists:
                #     file = fileDB.session.query(File).filter(File.filename == filename).one()
                #     file.data = data
                #     fileDB.session.commit()
                # else:
                file = File(filename=filename, data=data) #Create a 'File' object of the File class in the DATABASE CONFIGURATION part of the code
                fileDB.session.add(file)
                fileDB.session.commit()

                #Save locally (for testing purposes)
                #f.save(os.getcwd() + '\\Uploads\\' + filename)

            producer.send('coordinatorToClassifier', value={'signal': "startClassifier"})

            return "ok"
        else:
            return errorType
    except Exception:
        return Exception

if __name__ == "__main__":
    # classifierListener = Thread(target= listen, args=[consumerClassifier, "classifierDone", producer, 'coordinatorToLocalizer', {'signal': "startLocalizer"}])
    # classifierListener.start()
    
    #app.run(debug=True)

    while True:
        msg=consumerListener.poll(1.0) #timeout
        if msg is None:
            continue
        if msg.error():
            print('Error: {}'.format(msg.error()))
            continue
        data=msg.value().decode('utf-8')
        print(data)
    consumerListener.close()