import LoaderComp from "./Loader";
import './styles.css';
import { useEffect, useState } from "react";
import axios from 'axios';

// const Loading = () => {

//     axios.get('http://localhost:5000/loading').then( (e) => {
//         console.log(e.data)
//     })
//     .catch( (e) => {
//         console.error('Error: ', e)
//         alert('Error: ' + e)
//     })
    

//     return ( 
//         <div className="container">
//             <h1 style={{marginBottom: 25}}>Loading Results</h1>
//             <LoaderComp/>
//         </div>
//     );
// }

// export default Loading;

function Loading() {
    const [data, setData] = useState([]);
  
    useEffect(() => {

        const fetchConfirmation = async () => {
            axios.get('http://localhost:5000/loading', data)
                .then( (e) => {
                    console.log(e.data)
                })
                .catch( (e) => {
                    console.error('Error: ', e)
                    alert('Error: ' + e)
                })
        };

        fetchConfirmation();
    }, []);
  
    return ( 
        <div className="container">
            <h1 style={{marginBottom: 25}}>Loading Results</h1>
            <LoaderComp/>
        </div>
    );
}

export default Loading;