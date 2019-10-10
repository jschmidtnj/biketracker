import React from 'react'
import { connect } from 'mqtt'
import { ToastContainer, toast } from 'react-toastify'
import GoogleMapReact from 'google-map-react'
import $ from 'jquery'
import 'bootstrap/dist/js/bootstrap.bundle.min.js'
import 'react-toastify/dist/ReactToastify.min.css'
import './style.scss'

const defaultLatitude = 40.742702
const defaultLongitude = -74.027167
const defaultZoom = 13

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

class TheMap extends React.Component {
  constructor(props) {
    super(props)
    this.state = {
      map: null,
    }
  }
  componentDidUpdate(prevProps) {
    if (
      this.state.map !== null &&
      (this.props.lat !== prevProps.lat || this.props.lgn !== prevProps.lgn)
    )
      this.state.map.setCenter({ lat: this.props.lat, lng: this.props.lgn })
  }
  render() {
    const Marker = () => {
      return (
        <span className="location" data-toggle="tooltip" title="bike"></span>
      )
    }
    return (
      <GoogleMapReact
        bootstrapURLKeys={{ key: process.env.GATSBY_GOOGLE_MAPS_API_KEY }}
        defaultCenter={{ lat: defaultLatitude, lng: defaultLongitude }}
        defaultZoom={defaultZoom}
        yesIWantToUseGoogleMapApiInternals
        onGoogleApiLoaded={({ map }) => (this.state.map = map)}
      >
        <Marker lat={this.props.lat} lng={this.props.lgn} />
      </GoogleMapReact>
    )
  }
}

class Controller extends React.Component {
  constructor(props) {
    super(props)
    this.state = {
      location: [-1, -1, -1, -1, -1],
      weather: [-1, -1, -1, -1],
      battery: [-1, -1, -1, -1],
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
            location: locationData,
          })
          break
        case topics.weather:
          const weatherData = messageStr.split(',')
          this.setState({
            weather: weatherData,
          })
          break
        case topics.battery:
          const batteryData = messageStr.split(',')
          this.setState({
            battery: batteryData,
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
      <div style={{ marginBottom: '2vh' }}>
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
        <div style={{ height: '92.5vh', width: '100%' }}>
          <TheMap lat={this.state.location[1]} lgn={this.state.location[2]} />
        </div>
        <table className="table table-hover table-dark">
          <tbody>
            <tr>
              <th scope="row">latitude</th>
              <td>{this.state.location[1]}°</td>
            </tr>
            <tr>
              <th scope="row">longitude</th>
              <td>{this.state.location[2]}°</td>
            </tr>
            <tr>
              <th scope="row">speed</th>
              <td>{this.state.location[0]} km/hr</td>
            </tr>
            <tr>
              <th scope="row">heading</th>
              <td>{this.state.location[3]}°</td>
            </tr>
            <tr>
              <th scope="row">altitude</th>
              <td>{this.state.location[4]} m</td>
            </tr>
            <tr>
              <th scope="row">temperature</th>
              <td>{this.state.weather[0]}°C</td>
            </tr>
            <tr>
              <th scope="row">pressure</th>
              <td>{this.state.weather[1]} hPa</td>
            </tr>
            <tr>
              <th scope="row">altitude</th>
              <td>{this.state.weather[2]} m</td>
            </tr>
            <tr>
              <th scope="row">humidity</th>
              <td>{this.state.weather[3]}%</td>
            </tr>
            <tr>
              <th scope="row">voltage</th>
              <td>{this.state.battery[0]} V</td>
            </tr>
            <tr>
              <th scope="row">current</th>
              <td>{this.state.battery[1]} A</td>
            </tr>
            <tr>
              <th scope="row">power</th>
              <td>{this.state.battery[2]} W</td>
            </tr>
            <tr>
              <th scope="row">battery</th>
              <td>{this.state.battery[3]}%</td>
            </tr>
          </tbody>
        </table>
      </div>
    )
  }
}

export default Controller
