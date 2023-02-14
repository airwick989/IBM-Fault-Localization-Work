import os, time
from threading import Thread

filename = "Hot_1"
args = "4 100"

start_time = "15"
recording_length = "20"

delay = 1
script_running_time = delay + int(start_time) + int(recording_length) + delay

def run_rtdriver():
    time.sleep(delay)
    os.system(f"./run_rtdriver.sh {start_time} {recording_length}")

def run_jlm():
    os.system(f"./run_jlm.sh {script_running_time} {filename} {args}")

rtdriver = Thread(target= run_rtdriver)
jlm = Thread(target=run_jlm)
rtdriver.start()
jlm.start()

time.sleep(script_running_time)
rtdriver.join()
jlm.join()