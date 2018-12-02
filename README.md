# AI-for-Set-Operations


## Dependencies
##### Debian (dpkg)
```
sudo apt-get update 

//Boost
sudo apt-get install libboost-all-dev
```
##### Red Hat (RPM) 
``` 
sudo yum update
sudo yum install epel-release

//Boost
sudo yum install boost boost-thread boost-devel boost-filesystem
```


## Usage: Build datasets
```
mkdir build && cd build

cmake ..
make -j13 -o3
./CDataHustle
```

## Machine Learning
Start jupyter Notebook
``` 
cd "Machine Learning"
jupyter notebook
```
