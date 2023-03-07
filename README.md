# Software Lock Contention and Fault Localization Recommender System (Prototype Build)

## Current System Architecture
![Current System Architecture](./md_images/curr_sys_arch.png "Current System Architecture")

### Modules and Responsibilities
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
- SQLite database stored locally in the system.
- Stores files to be shared across multiple modules.
- Stores files as binary objects to be encoded and decoded.
- Details of the 'Common Data Store' found on [Database Configuration](##database-configuration) section.

## Database Configuration
