import React, {useState, useEffect} from 'react'
// eslint-disable-next-line
import styles from "./index.css"  //This import is important, ignore the warning

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

      <h1 className="text-4xl font-bold underline italic">Hello there</h1>
      

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