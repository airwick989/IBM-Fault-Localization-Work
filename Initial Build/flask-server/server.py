from pickle import TRUE
from flask import Flask
import json

app = Flask(__name__)

# Members API Route
@app.route("/members")
def members():
    # members = {"Member 1", "Member 2", "Member 3"}
    # return json.dumps(list(members))

    return {"members": ["Member 1", "Member 2", "Member 3"]}

if __name__ == "__main__":
    app.run(debug=True)