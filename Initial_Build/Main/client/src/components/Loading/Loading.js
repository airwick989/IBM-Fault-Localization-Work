import LoaderComp from "./Loader";
import './styles.css';
import { useEffect, useState } from "react";
import { useHistory } from "react-router-dom";
import axios from 'axios';

const Loading = () => {
    const history = useHistory();

    axios.get('http://localhost:5000/loading').then( (e) => {
        console.log(e.data)
        if(e.data === "complete"){
            console.log("very nice")
        }
        else{
            //alert('Files Uploaded Successfully')
            //history.push('/');
        }
    })
    .catch( (e) => {
        console.error('Error: ', e)
        alert('Error: ' + e)
    })
    

    return ( 
        <div className="container">
            <h1 style={{marginBottom: 25}}>Loading Results</h1>
            <LoaderComp/>
        </div>
    );
}

export default Loading;