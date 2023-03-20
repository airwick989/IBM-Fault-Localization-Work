import os, time, re
from threading import Thread


#Clearing out the log directory
if len(os.listdir('./logs/')) == 0:
    pass
else:    
    for file in os.listdir('./logs/'):
        os.remove(f'./logs/{file}')


filename = "./Files/Hot_1.jar"
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


#Returning the method causing lock contention
methods = []
r = re.compile("^log-rt")
log_rt_file = list(filter(r.match, os.listdir('./logs/')))[0]
prevLine = ""
with open(f'./logs/{log_rt_file}') as file:
    flag = False
    for line in file:
        if line.split() == ['LV', 'EVENT', 'NAME']:
            flag = True

        if flag:
            if line.split()[1] not in ['0', 'EVENT']:
                # print(line.split())
                # print(prevLine)
                # print(prevLine.split())
                methods.append(prevLine.split()[2])

        prevLine = line

methods = list(set(methods))
print("\n\n")
methods_str = "The method(s) causing contention in your Java program are: "
for method in methods:
    methods_str = methods_str + method + ", "
print(methods_str)