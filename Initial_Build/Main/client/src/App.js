// eslint-disable-next-line
import React, {useState, useEffect} from 'react'
// eslint-disable-next-line
import styles from "./index.css"  //This import is important, ignore the warning
import { BrowserRouter as Router, Routes,Route } from 'react-router-dom';
import Home from './Home';
import Loading from './components/Loading/Loading';
import LocalizationResults from './components/LocalizationResults/LocalizationResults';
import EndResults from './components/EndResults/EndResults';




function App(){

  // const [data, setData] = useState([{}])

  // //Once the application is run, the useEffect block is run once
  // useEffect(() => {
  //   //Fetch response from members endpoint
  //   fetch("/members").then(
  //     //whatever response we get, convert it to json
  //     res => res.json()
  //   ).then(
  //     data => {
  //       //whatever data is in that response json, we're gonna set that data to the "data" variable using the setData function
  //       setData(data)
  //       console.log(data) //Checking that we were able to retrieve the data from the backend
  //     }
  //   )
  // }, [])

  return (

    <Router>

      <div className='App'>

        <div className='Content'>
          <Routes>
              {/* Home page */}
              <Route exact path="/" element={<Home/>}/>
              {/* Loading page */}
              <Route path="/loading" element={<Loading/>}/>
              {/* Localization Results page */}
              <Route path="/localizationResults" element={<LocalizationResults title="Results Until Fault Localization" hasUploader={true}/>}/>
              {/* End Results page */}
              <Route path="/endResults" element={<EndResults/>}/>
          </Routes>
        </div>
      </div>

    </Router>
  )
}

export default App