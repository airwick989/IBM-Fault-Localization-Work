# Software Lock Contention and Fault Localization Recommender System (Prototype Build)

## Current System Architecture
![Current System Architecture](./md_images/curr_sys_arch.png "Current System Architecture")

### Brief Summary of Modules and their Responsibilities
#### Coordinator
- Acts as primary interface for the user to the system.
- Where the user enters the necessary inputs to the system.
- Types of inputs accepted:
  - '.java' file (Java program) (must be uploaded).
  - 'jlm.csv' (jlm metrics) (optional).
  - 'perf.csv' (perf metrics) (optional).
  - 'test.csv' (test metrics) (optional).
#### Performance Benchmarking
- Responsible for executing a performance benchmarking of the uploaded Java program.
- Records, collects, and stores performance metrics.
- Currently a black box in the system (area of investigation).
#### Common Data Store
- Database stored locally in the system.
- Stores files to be shared across multiple modules.
- Stores files as binary objects to be encoded and decoded.
#### Topic-based Pub/Sub Broker
- Used to have system exhibit an event-driven behaviour.
- Responsible for calling on module functionality when initiated by the Coordinator.
- Sends completion signals back to the Coordinator upon a module completing its designated functionality.
- Pub/sub model is in place to ensure each module is more discrete and separated from the rest of the system as development is occurring within the functionalities of various modules at any given moment (high cohesion, low coupling).
#### lcClassifier
- Lock contention classifier.
- Uses all the performance metrics collected from the Java program and runs it against a pre-trained machine learning classifier.
- Classification returns 1 of 3 possible lock contention types:
  - Type 0: Minimal or no lock contention exhibited.
  - Type 1: A thread(s) is holding the lock to a critical section for a prolonged time.
  - Type 2: High frequency of access requests from threads to acquire a particular lock.
#### crLocator
- Contented region locator.
- Executes the Java program and simultaneously performs call stack tracing.
- Produces and automatically parses a log file containing the resultant call stack traces.
- Localize the fault by finding the method causing contention.
#### apIdentifier
- Anti-pattern identifier.
- Detects anti-patterns (common bad practices) in the Java code which cause contention.
- Returns recommendations to resolve the issues identified.
- Currently a work-in-progress. 

## Coordinator Module
### Tools & Technologies Used
- React (JS)
  - Currently runs on port 3000 of the localhost (http://localhost:3000)
  - Acts as the front-end of the coordinator module.
- Flask (Python)
  - Currently runs on port 5000 of the localhost (http://localhost:5000)
  - Is the backend of the coordinator module.
### Front-end Functionality
- Main landing page is in [Home.js](./Initial_Build/Coordinator/client/src/Home.js).
- 'Home.js' primarily utilizes and displays the [FileUploader component](./Initial_Build/Coordinator/client/src/components/FileUploader/index.js).
- It is responsible for displaying information, receiving user input, preliminary error-checking, and notifying the user of certain events.
- As it currently stands, the front-end is set up such that it **requires** the 3 CSV files along with the Java program. This is because the performance benchmarking is currently an area of investigation. 
- After ensuring the uploaded files meet some specified criteria, it uploads the files to the coordinator's backend using an HTTP POST method.
- It has error messages which may be returned to the user if the backend returns some error type.
### Backend Functionality
- Hello there

## Performance BenchMarking
### This portion of the system is currently a work-in-progress

## Common Data Store

## Topic-based Pub/Sub Broker

## Lock Contention Classifier

## Contented Region Locator

## Anti-pattern Identifier
### This portion of the system is currently a work-in-progress