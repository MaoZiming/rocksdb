import matplotlib.pyplot as plt
import re
from datetime import datetime
import seaborn as sns
# Path to the log file
# log_file = "build/cpu_2024-09-05 05:59:40.log"
# output_file = 'plot/Poisson-TTL.pdf'

# log_file = "build/cpu_2024-09-05 05:56:16.log"
# output_file = 'plot/Poisson-Adaptive.pdf'

# log_file = "build/cpu_2024-09-05 06:24:34.log"
# output_file = 'plot/Poisson-TTL.pdf'

# log_file = "build/cpu_2024-09-05 06:32:10.log"
# output_file = 'plot/Poisson-Adaptive.pdf'

# log_file = "build/cpu_2024-09-05 07:08:26.log"
# output_file = 'plot/Poisson-TTL-uniform.pdf'

log_file = "build/cpu_2024-09-05 07:12:50.log"
output_file = 'plot/Poisson-Adaptive-uniform.pdf'

log_file = "build/cpu_2024-09-05 07:25:58.log"
output_file = "plot/Poisson-Update-uniform.pdf"

log_file = "build/cpu_2024-09-05 07:38:12.log"
output_file = "plot/Poisson-Update-with-comp.pdf"

log_file = "build/cpu_2024-09-05 07:44:08.log"
output_file = "plot/Poisson-Update.pdf"

log_file = "build/cpu_2024-09-05 07:49:35.log"
output_file = "plot/Poisson-Invalidate-uniform.pdf"

log_file = "build/cpu_2024-09-05 07:53:45.log"
output_file = "plot/Poisson-TTL-uniform.pdf"

log_file = "build/cpu_2024-09-05 17:41:30.log"
output_file = "plot/PoissonMix-TTL.pdf"

log_file = "build/cpu_2024-09-05 17:45:10.log"
output_file = "plot/PoissonMix-Invalidate.pdf"

log_file = "build/cpu_2024-09-05 17:49:43.log"
output_file = "plot/PoissonMix-Update.pdf"

log_file = "build/cpu_2024-09-06 06:16:30.log"
output_file = "plot/Meta-TTL.pdf"

log_file = "build/cpu_2024-09-06 06:23:21.log"
output_file = "plot/Meta-Invalidate.pdf"

log_file = "build/cpu_2024-09-06 06:30:26.log"
output_file = "plot/Meta-Update.pdf"

import matplotlib.dates as mdates
from datetime import timedelta

BIG_SIZE= 10
FIGRATIO = 1 / 3
FIGWIDTH = 7 # inches
FIGHEIGHT = FIGWIDTH * FIGRATIO
FIGSIZE = (FIGWIDTH, FIGHEIGHT)

plt.rcParams.update(
{
"figure.figsize": FIGSIZE,
"figure.dpi": 300,
}
)

COLORS = sns.color_palette("Paired")
sns.set_style("ticks")
sns.set_palette(COLORS)

plt.rc("font", size=BIG_SIZE) # controls default text sizes
plt.rc("axes", titlesize=BIG_SIZE) # fontsize of the axes title
plt.rc("axes", labelsize=BIG_SIZE) # fontsize of the x and y labels
plt.rc("xtick", labelsize=BIG_SIZE) # fontsize of the tick labels
plt.rc("ytick", labelsize=BIG_SIZE) # fontsize of the tick labels
plt.rc("legend", fontsize=BIG_SIZE) # legend fontsize
plt.rc("figure", titlesize=BIG_SIZE) # fontsize of the figure title

# Lists to store extracted data
timestamps = []
cpu_utilizations = []
network_recv_mb = []
network_send_mb = []

# Regular expression to parse the log line
log_pattern = re.compile(
    r"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) - CPU Utilization: ([\d.]+)% \| "
    r"usr: [\d.]+%, sys: [\d.]+%, idle: [\d.]+%, iowait: [\d.]+%, steal: [\d.]+% \| "
    r"Network recv: (\d+) bytes, send: (\d+) bytes"
)

has_warmed = False
# Read and process the log file
with open(log_file, "r") as file:
    for line in file:
        match = log_pattern.match(line)
        if match:
            timestamp_str = match.group(1)
            cpu_utilization = float(match.group(2))
            recv_bytes = int(match.group(3)) / (1024 * 1024)  # Convert to MB
            send_bytes = int(match.group(4)) / (1024 * 1024)  # Convert to MB
            
            # Convert the timestamp string to a datetime object
            timestamp = datetime.strptime(timestamp_str, "%Y-%m-%d %H:%M:%S")
            
            # Append the data to lists
            timestamps.append(timestamp)
            cpu_utilizations.append(cpu_utilization)
            network_recv_mb.append(recv_bytes)
            network_send_mb.append(send_bytes)

start_time = timestamps[0]

relative_times = [(ts - start_time).total_seconds() for ts in timestamps]
print(relative_times)
# end_time = start_time + timedelta(minutes=2.5)

# Create the figure and axis objects
fig, ax1 = plt.subplots()


# Plot network statistics on the primary y-axis
ax1.plot(relative_times, network_recv_mb, label="Recv (MB/s)", color="green")
ax1.plot(relative_times, network_send_mb, label="Send (MB/s)", color="red")

# Label and format the primary y-axis (network)
ax1.set_xlabel("Time")
ax1.set_ylabel("Network Traffic (MB/s)", color="black")
ax1.tick_params(axis='y')

# Format x-axis to show minutes
ax1.xaxis.set_major_locator(plt.MaxNLocator(integer=True))  # Ensure that x-axis ticks are at whole minutes
ax1.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, _: f"{int(x)} s"))  # Format x-axis as minutes

ax1.set_xlim([0, 350])

# Format x-axis to prevent crowding
# ax1.xaxis.set_major_locator(mdates.AutoDateLocator())  # Automatically manage x-axis ticks
# ax1.xaxis.set_major_formatter(mdates.DateFormatter('%M\'%S\'\''))  # Format to show date and time
# plt.xticks(rotation=45, ha='right')  # Rotate and align x-axis labels


# Create a secondary y-axis for CPU utilization
ax2 = ax1.twinx()
ax2.plot(relative_times, cpu_utilizations, label="CPU Utilization (%)", color="blue", linestyle="--")
ax2.set_ylabel("CPU Utilization (%)", color="blue")
ax2.tick_params(axis='y', labelcolor="blue")

# Formatting the plot
# fig.suptitle("CPU Utilization and Network Traffic over Time", fontsize=14)
ax1.legend(loc="upper left")
ax2.legend(loc="upper right")
plt.xticks(rotation=45)
ax1.grid(True)

# Show the plot
plt.tight_layout()
# plt.show()
plt.savefig(output_file)
