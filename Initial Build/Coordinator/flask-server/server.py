from flask import Flask, request
from flask_cors import CORS;
from werkzeug.utils import secure_filename
from flask_sqlalchemy import SQLAlchemy
from time import sleep
from json import dumps
from kafka import KafkaProducer

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


"""---- KAFKA PRODUCER ------------------------------------------------------------------------------------------------------------------"""

producer = KafkaProducer(
    bootstrap_servers = ['localhost:9092'],
    value_serializer = lambda x:dumps(x).encode('utf-8')
)

"""---- KAFKA PRODUCER ------------------------------------------------------------------------------------------------------------------"""


@app.route("/upload", methods=['GET', 'POST'])
def upload():

    accepted_filenames = ['jlm.csv', 'perf.csv', 'test.csv']

    try:
        files = request.files.getlist("file")
        flag = True
        for f in files:
            filename = secure_filename(f.filename)
            if filename not in accepted_filenames:
                flag = False
                break
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

        if flag == True:

            message = {'data': "somedata"}
            producer.send('testTopic', value=message)

            return "ok"
        else:
            return "fileNameError"
    except Exception:
        return Exception

if __name__ == "__main__":
    app.run(debug=True)