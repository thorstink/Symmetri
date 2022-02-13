FROM ubuntu
LABEL author=thomashorstink@gmail.com

RUN apt-get -y update
RUN DEBIAN_FRONTEND=noninteractive && apt-get install -y tzdata
RUN DEBIAN_FRONTEND=noninteractive && apt-get install -y --no-install-recommends build-essential wget gcc g++ libgmp-dev libgmpxx4ldbl libboost-all-dev \
    flexc++ ant cmake libfl-dev libglib2.0-dev patch python3 libglpk-dev liblpsolve55-dev \
    autoconf automake libtool zip flex byacc time graphviz libsuitesparse-dev \
    libmotif-dev make libxml++2.6-dev libglibmm-2.4-dev texlive-font-utils \
    default-jdk git x11-apps
# RUN apt-get install -qqy x11-apps

# RUN git clone  https://github.com/asminer/meddly.git meddly \
#     && cd meddly \
#     && ./autogen.sh \
#     && ./configure --prefix=/usr/local \
#     && make -j4 install

# RUN wget http://www.lrde.epita.fr/dload/spot/spot-2.9.6.tar.gz \
#     && tar xzf spot-2.9.6.tar.gz \
#     && cd spot-2.9.6 \
#     && ./configure # --disable-python \
#     && make -j4 install

RUN git clone https://github.com/ogdf/ogdf \
    && cd ogdf \
    && ./makeMakefile.sh \
    && mkdir build && cd build && cmake .. && make -j4\
    && make install

RUN mkdir ~/GreatSPN \
    && cd ~/GreatSPN \
    && git clone https://github.com/greatspn/SOURCES.git SOURCES \
    && cd ~/GreatSPN/SOURCES \
    && make -j4 \
    && make install

ENV JAVA_HOME="/usr/lib/jvm/open-jdk"
CMD java -jar ~/GreatSPN/SOURCES/objects/JavaGUI/bin/Editor.jar
