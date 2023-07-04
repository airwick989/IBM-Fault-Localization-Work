import LoaderComp from "./Loader";
import './styles.css';
import { useEffect } from "react";
import axios from 'axios';
import { useNavigate } from "react-router-dom";

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

    const navigate = useNavigate();
  
    useEffect(() => {

        const fetchConfirmation = async () => {
            axios.get('http://localhost:5000/loading')
                .then( (e) => {
                    console.log(e.data)
                    if(e.data === "completed"){
                        navigate("/localizationResults");
                    }
                    else{
                        alert(e.data);
                        navigate("/");
                    }
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
            <h1 class="text-5xl font-bold" style={{marginBottom: 50}}>Loading Results</h1>
            <LoaderComp/>
            <a href="/">
                <button className="btn btn-sm">Return Home</button>
            </a>
        </div>
    );
}

export default Loading;