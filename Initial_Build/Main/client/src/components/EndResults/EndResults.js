import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';
import LocalizationResults from "../LocalizationResults/LocalizationResults";

function EndResults() {
    const [sourceFiles, setSourceFiles] = useState([]);
    const [synchRegions, setSynchRegions] = useState([]);
    const [aps, setAps] = useState({});


    useEffect(() => {

        const fetchResults = async () => {
            axios.get('http://localhost:5001/cds/getData', { params: { target: 'pattern_matcher.json', isMultiple: 'false' }})
            .then( (e) => {
                setSourceFiles(e.data['files']);
                setSynchRegions(e.data['synch_regions']);
                setAps(e.data['anti_patterns']);
            })
            .catch( (e) => {
                console.error('Error: ', e)
                alert('Error: ' + e)
            })
        };

        fetchResults();
    }, []);

    
    return ( 
        <div className="container" style={{overflowY: 'scroll'}}>
            <LocalizationResults/>
        </div>
    );
}

export default EndResults;