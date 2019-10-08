import React from 'react'
import { connect } from 'mqtt'
import { ToastContainer, toast } from 'react-toastify'
import GoogleMapReact from 'google-map-react'
import 'react-toastify/dist/ReactToastify.min.css'
import './style.scss'

const topics = {
  location: 'location',
  weather: 'weather',
  battery: 'battery',
  command: 'command',
  error: 'error',
}

const pollingRate = 1 // times per minute

const mqttOptions = {
  port: process.env.GATSBY_MQTT_PORT,
  host: process.env.GATSBY_MQTT_HOST,
  clientId:
    'mqttjs_' +
    Math.random()
      .toString(16)
      .substr(2, 8),
  username: process.env.GATSBY_MQTT_USERNAME,
  password: process.env.GATSBY_MQTT_PASSWORD,
  keepalive: 60,
  reconnectPeriod: 1000,
  clean: true,
  encoding: 'utf8',
}

const defaultLatitude = 59.95
const defaultLongitude = 30.33
const defaultZoom = 11

class Controller extends React.Component {
  constructor(props) {
    super(props)
    this.state = {
      location: {
        speed: 0,
        latitude: Object.create(0, defaultLatitude),
        longitude: Object.create(0, defaultLongitude),
        altitude: 0,
        heading: 0,
      },
      weather: {
        temperature: 0,
        pressure: 0,
        altitude: 0,
        humidity: 0,
      },
      battery: {
        voltage: 0,
        current: 0,
        power: 0,
        battery: 0,
      },
      pollingInterval: null,
    }
  }

  componentDidMount() {
    $('[data-toggle="tooltip"]').tooltip()
    this.state.client = connect(
      process.env.GATSBY_MQTT_HOST,
      mqttOptions
    )
    this.state.client.on('disconnect', () => {
      console.log('reconnect')
      this.state.client.reconnect()
    })
    this.state.client.on('connect', () => {
      console.log('connected!')
      this.state.client.subscribe(topics.location, err => {
        if (!err) console.log('subscribed to location topic')
        else
          toast.error(
            `error connecting to location topic: ${JSON.stringify(err)}`
          )
      })
      this.state.client.subscribe(topics.weather, err => {
        if (!err) console.log('subscribed to weather topic')
        else
          toast.error(
            `error connecting to weather topic: ${JSON.stringify(err)}`
          )
      })
      this.state.client.subscribe(topics.battery, err => {
        if (!err) console.log('subscribed to battery topic')
        else
          toast.error(
            `error connecting to battery topic: ${JSON.stringify(err)}`
          )
      })
      this.state.client.subscribe(topics.error, err => {
        if (!err) console.log('subscribed to error topic')
        else
          toast.error(`error connecting to error topic: ${JSON.stringify(err)}`)
      })
    })
    this.state.client.on('message', (topic, message) => {
      // message is Buffer
      const messageStr = message.toString()
      console.log(`${topic}: ${messageStr}`)
      switch (topic) {
        case topics.error:
          toast.error(messageStr)
          break
        case topics.location:
          const locationData = messageStr.split(',')
          this.setState({
            location: {
              speed: locationData[0],
              latitude: locationData[1],
              longitude: locationData[2],
              altitude: locationData[3],
              heading: locationData[4],
            },
          })
          break
        case topics.weather:
          const weatherData = messageStr.split(',')
          this.setState({
            weather: {
              temperature: weatherData[0],
              pressure: weatherData[1],
              altitude: weatherData[2],
              humidity: weatherData[3],
            },
          })
          break
        case topics.battery:
          const batteryData = messageStr.split(',')
          this.setState({
            battery: {
              voltage: batteryData[0],
              current: batteryData[1],
              power: batteryData[2],
              battery: batteryData[3],
            },
          })
          break
        default:
          console.log(messageStr)
          break
      }
    })
    this.state.client.publish(topics.command, 'connect', {}, err => {
      if (err)
        toast.error(`got error with connect command: ${JSON.stringify(err)}`)
    })
    this.state.pollingInterval = setInterval(() => {
      this.state.client.publish(topics.command, 'poll', {}, err => {
        if (err)
          toast.error(`got error with polling command: ${JSON.stringify(err)}`)
      })
    }, pollingRate * 1000 * 60)
  }

  componentWillUnmount() {
    clearInterval(this.state.pollingInterval)
  }

  render() {
    return (
      <div>
        <ToastContainer
          position="top-right"
          autoClose={5000}
          hideProgressBar={false}
          newestOnTop={false}
          closeOnClick
          rtl={false}
          pauseOnVisibilityChange
          draggable
          pauseOnHover
        />
        <div style={{ height: '100vh', width: '100%' }}>
          <GoogleMapReact
            bootstrapURLKeys={{ key: process.env.GATSBY_GOOGLE_MAPS_API_KEY }}
            defaultCenter={{
              lat: this.state.location.latitude,
              lgn: this.state.location.longitude,
            }}
            defaultZoom={defaultZoom}
          >
            <span
              lat={this.state.location.latitude}
              lng={this.state.location.longitude}
              className="location"
              data-toggle="tooltip"
              title="bike"
            ></span>
          </GoogleMapReact>
        </div>
        <div>{this.state.weather}</div>
        <div>{this.state.battery}</div>
      </div>
    )
  }
}

export default Controller
