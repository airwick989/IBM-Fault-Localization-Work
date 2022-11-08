from pickle import TRUE
from flask import Flask, request
from flask_cors import CORS;
import json

app = Flask(__name__)
CORS(app)

@app.route("/upload", methods=['GET', 'POST'])
def upload():
    print(request.files)

    return "ok"

if __name__ == "__main__":
    app.run(debug=True)