# syntax=docker/dockerfile:1
FROM python:3.9.7-slim
WORKDIR /pomcpp

# build dependencies
RUN apt-get update \
    && apt-get install g++ git cmake git zlib1g-dev libjpeg-dev -y \
    && apt-get clean

# disable pip cache
ENV PIP_NO_CACHE_DIR=1

# install pommerman environment
RUN git clone https://github.com/MultiAgentLearning/playground \
    && pip install -e playground/.

# copy source code
COPY src src
COPY include include
COPY CMakeLists.txt CMakeLists.txt

# build pomcpp library
RUN mkdir -p build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && make pomcpp_lib -j$(nproc) \
    && mv libpomcpp.so .. \
    && cd .. \
    && rm -r build

# install local python library
COPY py/ py/
RUN pip install -e py/.

ENTRYPOINT ["python", "/pomcpp/py/pypomcpp/cppagent_runner.py"]
