import numpy as np
from matplotlib import pyplot as plt

def parse_logfile(filename):
    """
    Read the logfile and get x,y value arrays

    Args:
        filename (str): the path of the logfile

    Returns:
        (x, y): the x and y arrays
    """

    with open(filename) as f:
        data = f.read()

    data = data.split("\n", 4)[4].split("\n\n")
    ssid = data[0].splitlines()

    time = []
    latency = []

    for j, dataline in enumerate(ssid):
        datalinestr = dataline.replace(')', '')
        datalinestr = [item.strip() for item in datalinestr.split('(')]

        if j > 0:
            time.append(datalinestr[0])
            latency.append(datalinestr[1])

    x = np.asarray(time, dtype=float)
    y = np.asarray(latency, dtype=float)

    x = x - x[0]  # x: start from 0
    y = y * 1000000  # y: convert in usec

    return x, y

def generate_plot(x, y):
    """
    Given x and y arrays, generate a plot

    Args:
        x (double array): the time values in sec
        y (double array): the latency values in usec
    """
    
    figure = plt.figure(figsize=(10, 6))
    figure.suptitle('Timestamp - Latency', fontsize=24)
    axes = figure.add_subplot(1, 1, 1)
    axes.set_xlabel('[sec]', fontsize=18)
    axes.set_ylabel('[usec]', fontsize=18)
    axes.plot(x, y, '.-', linewidth=2, markersize=20, color="green")
    axes.set_xticks(x)
    axes.margins(0.04)

    figure.tight_layout()
    figure.subplots_adjust(top=0.88)
    figure.savefig('latency.png', bbox_inches='tight')

if __name__ == '__main__':
    x, y = parse_logfile('ssids.txt')
    generate_plot(x, y)
