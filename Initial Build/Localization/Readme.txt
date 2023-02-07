- The specific version of openjdk used is version 8/1.8.0_292/8u292-b10.
- The tar.gz file containing that version of openjdk can be found in the 'java' folder within the 'Initial Build' folder. 

- The following are installation instructions for a Linux Debian operating system:
	- Open a terminal in the aforementioned 'java' folder.
	- Enter this command: 'sudo tar xvf ./openlogic-openjdk-jre-8u292-b10-linux-x64.tar.gz -C /opt'
	- Enter: 'sudo update-alternatives --install /usr/bin/java java /opt/openlogic-openjdk-jre-8u292-b10-linux-x64/bin/java 1000'
	- Enter: 'sudo update-alternatives --config java'
	- Select the correct version (/opt/openlogic-openjdk-jre-8u292-b10-linux-x64/bin/java) by entering the corresponding number
	- Enter: 'java -version' to ensure the default jdk is changed to the correct one.

- The following instructions are for estimating the maximum throughput of the SynchronizedMethodBench program using BumbleBench:
	- Open a terminal in the folder 'Initial Build' -> 'Localization' -> 'Files'.
	- Enter this command: '/opt/openlogic-openjdk-jre-8u292-b10-linux-x64/bin/java -jar ./BumbleBench.jar SynchronizedMethodBench'