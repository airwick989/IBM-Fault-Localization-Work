import LoaderComp from "./Loader";
import './styles.css';

const Loading = () => {
    return ( 
        <div className="container">
            <h1 style={{marginBottom: 25}}>Loading Results</h1>
            <LoaderComp/>
        </div>
    );
}

export default Loading;