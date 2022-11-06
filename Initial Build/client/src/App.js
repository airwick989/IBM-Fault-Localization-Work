import React, {useState, useEffect} from 'react'
import './App.css'

function App(){

  const [data, setData] = useState([{}])

  //Once the application is run, the useEffect block is run once
  useEffect(() => {
    //Fetch response from members endpoint
    fetch("/members").then(
      //whatever response we get, convert it to json
      res => res.json()
    ).then(
      data => {
        //whatever data is in that response json, we're gonna set that data to the "data" variable using the setData function
        setData(data)
        console.log(data) //Checking that we were able to retrieve the data from the backend
      }
    )
  }, [])

  return (

    <div>

      <div class="hero min-h-screen bg-base-200">
        <div class="hero-content text-center">
          <div class="max-w-md">
            <h1 class="text-5xl font-bold">Hello there</h1>
            <p class="py-6">Provident cupiditate voluptatem et in. Quaerat fugiat ut assumenda excepturi exercitationem quasi. In deleniti eaque aut repudiandae et a id nisi.</p>
            <button class="btn btn-primary">Get Started</button>
          </div>
        </div>
      </div>

      {(typeof data.members == 'undefined') ? (
        <p>Loading ...</p>
      ) : (
        data.members.map((member, i) => (
          <p key={i}>{member}</p>
        ))
      )}

    </div>
  )
}

export default App