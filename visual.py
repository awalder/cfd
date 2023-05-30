import pandas as pd
import matplotlib.pyplot as plt

# Read the CSV data
data = pd.read_csv('data.csv', skipinitialspace=True)

# Plot the data
plt.figure(figsize=(10, 8))

plt.subplot(2, 2, 1)
plt.plot(data['Element'], data['data.velocity.x'], label='Velocity X')
plt.plot(data['Element'], data['data.velocity.y'], label='Velocity Y')
plt.title('Velocity')
plt.legend()

plt.subplot(2, 2, 2)
# plt.plot(data['Element'], data['data.externalForce.x'], label='External Force X')
# plt.plot(data['Element'], data['data.externalForce.y'], label='External Force Y')
plt.title('External Force')
plt.legend()

plt.subplot(2, 2, 3)
# plt.plot(data['Element'], data['data.pressure'], label='Pressure')
plt.plot(data['Element'], data['data.density'], label='Density')
plt.title('Pressure and Density')
plt.legend()

plt.subplot(2, 2, 4)
# plt.plot(data['Element'], data['data.temperature'], label='Temperature')
plt.title('Temperature')
plt.legend()

plt.tight_layout()
plt.show()
