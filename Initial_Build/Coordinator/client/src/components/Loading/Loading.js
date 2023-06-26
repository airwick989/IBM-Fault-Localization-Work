import LoaderComp from "./Loader";
import './styles.css';
import { useEffect, useState } from "react";
import { useHistory } from "react-router-dom";
import axios from 'axios';

const Loading = () => {
    const [res, setRes] = useState('');



    axios.get('http://localhost:5000/loading').then( (e) => {
        console.log(e.data)
    })
    .catch( (e) => {
        console.error('Error: ', e)
        alert('Error: ' + e)
    })

    // axios.post('http://localhost:5000/upload', data)
    // .then( (e) => {
    //     if(e.data === "ok"){
    //         console.log('Files Uploaded Successfully')
    //         alert('Files Uploaded Successfully')
    //         history.push('/loading'); 
    //         //window.location.reload();
    //     }
    //     else if(e.data === "FileNameError"){
    //         console.error('FileNameError: Please ensure you upload only CSV files and a Jar file with the correct names.')
    //         alert('FileNameError: Please ensure you upload only CSV files and a Jar file with the correct names.')
    //     }
    //     else if(e.data === "FileCountError"){
    //         console.error('FileCountError: Please enter EXACTLY 3 CSV files according to the specified naming conventions and EXACTLY 1 Jar file.')
    //         alert('FileCountError: Please enter EXACTLY 3 CSV files according to the specified naming conventions and EXACTLY 1 Jar file.')
    //     }
    //     else{
    //         console.error('Error: ', e)
    //         alert('Error: ' + e)
    //     }
    // })
    // .catch( (e) => {
    //     console.error('Error: ', e)
    //     alert('Error: ' + e)
    // })

    

    return ( 
        <div className="container">
            <h1 style={{marginBottom: 25}}>Loading Results</h1>
            <LoaderComp/>
        </div>
    );
}

export default Loading;