import os, time
from threading import Thread

filename = "Hot_1"
args = "4 100"

start_time = "15"
end_time = "20"

def run_rtdriver():
    time.sleep(1)
    os.system(f"./run_rtdriver.sh {start_time} {end_time}")

def run_jlm():
    os.system(f"./run_jlm.sh {filename} {args}")

Thread(target= run_rtdriver).start()
Thread(target=run_jlm).start()